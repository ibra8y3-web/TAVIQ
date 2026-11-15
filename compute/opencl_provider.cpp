#define CL_TARGET_OPENCL_VERSION 120
#include "taviq/gpu_provider.h"
#include <CL/cl.h>
#include <climits>
#include <cstddef>
#include <mutex>
#include <string>
#include <utility>
#include <vector>

namespace {
constexpr std::size_t kMaxElements = 16u * 1024u * 1024u;
struct DeviceRecord {
    cl_device_id handle{};
    std::string name;
    std::string backend;
    cl_ulong memoryBytes{};
};

std::vector<DeviceRecord> g_devices;
std::once_flag g_devicesOnce;

std::string deviceString(cl_device_id device, cl_device_info key, const char* fallback) {
    std::size_t length = 0;
    if (clGetDeviceInfo(device, key, 0, nullptr, &length) != CL_SUCCESS ||
        length == 0 || length > 64u * 1024u) return fallback;
    std::vector<char> buffer(length, '\0');
    if (clGetDeviceInfo(device, key, buffer.size(), buffer.data(), nullptr) != CL_SUCCESS)
        return fallback;
    while (!buffer.empty() && buffer.back() == '\0') buffer.pop_back();
    return std::string(buffer.begin(), buffer.end());
}

void enumerateDevices() {
    cl_uint platformCount = 0;
    if (clGetPlatformIDs(0, nullptr, &platformCount) != CL_SUCCESS || platformCount == 0) return;
    std::vector<cl_platform_id> platforms(platformCount);
    if (clGetPlatformIDs(platformCount, platforms.data(), nullptr) != CL_SUCCESS) return;

    for (cl_platform_id platform : platforms) {
        cl_uint count = 0;
        cl_device_type selectedType = CL_DEVICE_TYPE_GPU;
        cl_int status = clGetDeviceIDs(platform, selectedType, 0, nullptr, &count);
        if (status == CL_DEVICE_NOT_FOUND || count == 0) {
            selectedType = CL_DEVICE_TYPE_CPU;
            count = 0;
            status = clGetDeviceIDs(platform, selectedType, 0, nullptr, &count);
        }
        if (status != CL_SUCCESS || count == 0) continue;

        std::vector<cl_device_id> found(count);
        if (clGetDeviceIDs(platform, selectedType, count, found.data(), nullptr) != CL_SUCCESS)
            continue;
        for (cl_device_id device : found) {
            DeviceRecord item;
            item.handle = device;
            item.name = deviceString(device, CL_DEVICE_NAME, "OpenCL device");
            item.backend = selectedType == CL_DEVICE_TYPE_GPU
                ? "OpenCL GPU" : "OpenCL CPU fallback";
            (void)clGetDeviceInfo(device, CL_DEVICE_GLOBAL_MEM_SIZE,
                                  sizeof(item.memoryBytes), &item.memoryBytes, nullptr);
            g_devices.push_back(std::move(item));
            if (g_devices.size() >= 256) return;
        }
    }
}

const std::vector<DeviceRecord>& devices() {
    std::call_once(g_devicesOnce, [] {
        try { enumerateDevices(); }
        catch (...) { g_devices.clear(); }
    });
    return g_devices;
}

struct OpenClHandles {
    cl_context context{};
    cl_command_queue queue{};
    cl_program program{};
    cl_kernel kernel{};
    cl_mem left{};
    cl_mem right{};
    cl_mem output{};
    ~OpenClHandles() {
        if (output) clReleaseMemObject(output);
        if (right) clReleaseMemObject(right);
        if (left) clReleaseMemObject(left);
        if (kernel) clReleaseKernel(kernel);
        if (program) clReleaseProgram(program);
        if (queue) clReleaseCommandQueue(queue);
        if (context) clReleaseContext(context);
    }
};

int providerDeviceCount() noexcept {
    try {
        const auto count = devices().size();
        return count > static_cast<std::size_t>(INT_MAX) ? 0 : static_cast<int>(count);
    } catch (...) { return 0; }
}

int providerDeviceInfo(int index, taviq_compute_device_v1* output) noexcept {
    try {
        const auto& list = devices();
        if (!output || index < 0 || static_cast<std::size_t>(index) >= list.size()) return 1;
        const auto& item = list[static_cast<std::size_t>(index)];
        output->name = item.name.c_str();
        output->backend = item.backend.c_str();
        output->memory_bytes = static_cast<std::uint64_t>(item.memoryBytes);
        return 0;
    } catch (...) { return 1; }
}

int providerVectorAdd(int index, const float* left, const float* right,
                      float* output, std::size_t count) noexcept {
    if (count == 0) return 0;
    if (!left || !right || !output || count > kMaxElements) return 1;
    try {
        const auto& list = devices();
        if (index < 0 || static_cast<std::size_t>(index) >= list.size()) return 1;
        const cl_device_id device = list[static_cast<std::size_t>(index)].handle;
        const std::size_t bytes = count * sizeof(float);
        OpenClHandles handles;
        cl_int status = CL_SUCCESS;
        handles.context = clCreateContext(nullptr, 1, &device, nullptr, nullptr, &status);
        if (status != CL_SUCCESS || !handles.context) return 1;
        handles.queue = clCreateCommandQueue(handles.context, device, 0, &status);
        if (status != CL_SUCCESS || !handles.queue) return 1;

        static const char* kernelSource =
            "__kernel void taviq_vector_add(__global const float* a, "
            "__global const float* b, __global float* c) {\n"
            "  const size_t i = get_global_id(0);\n"
            "  c[i] = a[i] + b[i];\n"
            "}\n";
        const std::size_t sourceLength = std::char_traits<char>::length(kernelSource);
        handles.program = clCreateProgramWithSource(handles.context, 1,
                                                     &kernelSource, &sourceLength, &status);
        if (status != CL_SUCCESS || !handles.program) return 1;
        status = clBuildProgram(handles.program, 1, &device, nullptr, nullptr, nullptr);
        if (status != CL_SUCCESS) return 1;
        handles.kernel = clCreateKernel(handles.program, "taviq_vector_add", &status);
        if (status != CL_SUCCESS || !handles.kernel) return 1;

        handles.left = clCreateBuffer(handles.context, CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR,
                                       bytes, const_cast<float*>(left), &status);
        if (status != CL_SUCCESS || !handles.left) return 1;
        handles.right = clCreateBuffer(handles.context, CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR,
                                        bytes, const_cast<float*>(right), &status);
        if (status != CL_SUCCESS || !handles.right) return 1;
        handles.output = clCreateBuffer(handles.context, CL_MEM_WRITE_ONLY, bytes, nullptr, &status);
        if (status != CL_SUCCESS || !handles.output) return 1;

        status = clSetKernelArg(handles.kernel, 0, sizeof(cl_mem), &handles.left);
        if (status != CL_SUCCESS) return 1;
        status = clSetKernelArg(handles.kernel, 1, sizeof(cl_mem), &handles.right);
        if (status != CL_SUCCESS) return 1;
        status = clSetKernelArg(handles.kernel, 2, sizeof(cl_mem), &handles.output);
        if (status != CL_SUCCESS) return 1;

        const std::size_t globalSize = count;
        status = clEnqueueNDRangeKernel(handles.queue, handles.kernel, 1, nullptr,
                                        &globalSize, nullptr, 0, nullptr, nullptr);
        if (status != CL_SUCCESS) return 1;
        status = clEnqueueReadBuffer(handles.queue, handles.output, CL_TRUE, 0, bytes,
                                     output, 0, nullptr, nullptr);
        if (status != CL_SUCCESS) return 1;
        return clFinish(handles.queue) == CL_SUCCESS ? 0 : 1;
    } catch (...) { return 1; }
}

void providerShutdown() noexcept {}

const taviq_gpu_provider_v1 kProvider = {
    TAVIQ_COMPUTE_ABI_VERSION,
    "TAVIQ OpenCL provider",
    &providerDeviceCount,
    &providerDeviceInfo,
    &providerVectorAdd,
    &providerShutdown
};
} // namespace

extern "C" TAVIQ_COMPUTE_EXPORT
const taviq_gpu_provider_v1* taviq_compute_provider_entry_v1(void) {
    return &kProvider;
}
