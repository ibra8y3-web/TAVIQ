# TAVIQ GPU Provider Host

TAVIQ ships a host-side dynamic provider loader and an optional OpenCL implementation. The host ABI can enumerate devices and execute float32 vector addition through a trusted native provider.

## Provider ABI

A shared library exports taviq_compute_provider_entry_v1 and returns a pointer to a static taviq_gpu_provider_v1 table. ABI version must equal TAVIQ_COMPUTE_ABI_VERSION. Required callbacks are device_count, device_info and vector_add_f32; shutdown is optional.

The host validates provider ABI metadata and bounds vector length. Provider code is native code and executes with the host process privileges. Load only libraries from a trusted source.

## OpenCL provider

The optional module in compute/opencl_provider.cpp implements the existing ABI with actual OpenCL commands:

- Enumerates OpenCL GPU devices where available.
- Uses an OpenCL CPU device only as a reported fallback when a platform has no GPU device.
- Creates an OpenCL context, command queue, program, kernel and bounded input/output buffers for each vector-add operation.
- Releases OpenCL resources on success and failure.
- Limits each vector to 16 million float32 values.

OpenCL is optional and is not built into the default CLI or Android NDK build. Enable it with:

    cmake -S . -B build-opencl -DTAVIQ_ENABLE_OPENCL_PROVIDER=ON
    cmake --build build-opencl --parallel

On Debian/Ubuntu, install ocl-icd-opencl-dev and an ICD implementation/driver first. The module is linked to the host OpenCL loader; a compatible runtime/driver must also be installed where it is used.

## CLI

    build/taviq gpu-info /path/to/taviq_opencl_provider.so
    build/taviq gpu-add /path/to/taviq_opencl_provider.so 0 1,2,3 4,5,6

The second command invokes the provider's vector-add callback and prints the resulting comma-separated values. On a CPU fallback it is OpenCL compute, not hardware GPU acceleration.

## Validation

The optional integration test dynamically loads the built module and exercises vector addition on an available OpenCL device. If no OpenCL device is exposed, it reports a skip; CI installs POCL so that a real OpenCL CPU device can exercise the kernel. The existing ABI fixture test remains a separate host-contract test and is not represented as GPU hardware validation.

Production Vulkan, CUDA and Metal providers, physical GPU validation on user hardware, device-specific performance tests and GPU memory planning remain outstanding.
