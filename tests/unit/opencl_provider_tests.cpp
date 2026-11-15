#include "taviq/gpu.hpp"
#include <cmath>
#include <iostream>
#include <string>
#include <vector>
#ifndef TAVIQ_TEST_OPENCL_PROVIDER_PATH
#error TAVIQ_TEST_OPENCL_PROVIDER_PATH must point to the OpenCL provider module
#endif
#define CHECK(condition) do { if (!(condition)) { \
    std::cerr << "CHECK failed at " << __FILE__ << ":" << __LINE__ << ": " #condition "\n"; \
    return 1; \
} } while (false)

int main() {
    taviq::GpuProvider provider;
    std::string error;
    CHECK(provider.load(TAVIQ_TEST_OPENCL_PROVIDER_PATH, error));
    CHECK(provider.loaded());
    CHECK(provider.name() == "TAVIQ OpenCL provider");

    const int count = provider.deviceCount(error);
    CHECK(count >= 0);
    if (count == 0) {
        std::cout << "SKIP: no OpenCL GPU or CPU device is exposed by the installed ICD\n";
        return 0;
    }

    taviq::GpuDeviceInfo info;
    CHECK(provider.deviceInfo(0, info, error));
    CHECK(!info.name.empty());
    CHECK(info.backend == "OpenCL GPU" || info.backend == "OpenCL CPU fallback");

    const std::vector<float> left{1.0f, -2.5f, 3.25f, 100.0f};
    const std::vector<float> right{4.0f, 2.0f, -0.25f, -50.0f};
    std::vector<float> output;
    CHECK(provider.vectorAddF32(0, left, right, output, error));
    CHECK(output.size() == left.size());
    for (std::size_t i = 0; i < output.size(); ++i)
        CHECK(std::fabs(output[i] - (left[i] + right[i])) < 1.0e-5f);

    CHECK(provider.vectorAddF32(0, {}, {}, output, error));
    CHECK(output.empty());
    CHECK(!provider.vectorAddF32(count, left, right, output, error));
    CHECK(output.empty());
    CHECK(!provider.vectorAddF32(0, left, std::vector<float>{1.0f}, output, error));
    CHECK(output.empty());
    std::cout << "OpenCL vector-add validated on " << info.name
              << " (" << info.backend << ")\n";
    return 0;
}
