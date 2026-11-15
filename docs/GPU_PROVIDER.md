# TAVIQ GPU Provider Host

TAVIQ ships a host-side dynamic provider loader. An application or the CLI can explicitly load a provider module, inspect its ABI version and device list, and invoke the version-1 float32 vector-add operation.

## Provider ABI

A shared library exports taviq_compute_provider_entry_v1 and returns a pointer to a static taviq_gpu_provider_v1 table. ABI version must equal TAVIQ_COMPUTE_ABI_VERSION. Required callbacks are device_count, device_info and vector_add_f32. shutdown is optional and is called before unloading.

On Windows, provider modules must define TAVIQ_COMPUTE_PROVIDER_BUILD when compiling so the entry point is exported from the DLL. On POSIX platforms, the ABI header marks the entry point with default symbol visibility.

Provider operations return zero on success and non-zero on failure. Provider code is native code and is trusted: only load modules obtained from a trusted source. TAVIQ does not download, discover or execute provider libraries automatically.

## CLI

    build/taviq gpu-info /absolute/path/to/provider-library

The loader supports Windows LoadLibrary, POSIX dlopen platforms including Linux/macOS, and Android's dynamic linker. The project does not bundle a production Vulkan, CUDA or Metal provider yet.

## Contract tests

CI builds a test-only ABI fixture module and loads it through the same dynamic loader on host test runners. This validates the module boundary, ABI negotiation, metadata copying, error paths and vector-add call. It is not a hardware acceleration test.
