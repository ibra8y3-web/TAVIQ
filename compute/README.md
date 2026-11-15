# Compute Engine

The CPU parallel-for implementation is in parallel.cpp and uses C++20 threads. It divides a range into bounded chunks, propagates worker exceptions to the caller and joins all workers.

The C provider ABI is in taviq_gpu_provider.h. It defines a stable discovery and vector-add contract for future Vulkan, CUDA or Metal modules. No GPU module is shipped or loaded by this release; do not advertise hardware acceleration until a provider is implemented and tested on real devices.
