# Compute Engine

The CPU parallel-for implementation is in parallel.cpp and uses C++20 threads. It divides a range into bounded chunks, propagates worker exceptions and joins all workers.

The GPU provider ABI is public in include/taviq/gpu_provider.h. compute/gpu_provider.cpp implements an explicit, cross-platform dynamic-library loader with ABI version checks, device metadata validation and a bounded float32 vector-add call. The CLI exposes gpu-info for provider discovery.

CI builds a test-only provider module to validate the plugin boundary on Windows, Linux and macOS. This is a software ABI fixture only. No production Vulkan, CUDA or Metal module is included, and hardware acceleration remains unavailable until such a provider is implemented and validated on physical devices.
