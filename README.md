# TAVIQ — AI-Native Computing Language

TAVIQ is an evolving open-source programming-language and compute-runtime project.

## Working language core

- C++20 frontend with typed functions, parameters, return values, recursive calls, let/var, number/string/bool annotations, if/else and while.
- QVM v3 with real CALL/RETURN instructions, isolated frames, recursion and typed control-flow verification.
- QVM v1 and v2 loading compatibility.
- Portable C source lowering for the currently supported scalar/control-flow subset. User-defined functions run through QVM and are rejected by the C backend until function lowering is implemented.
- Formatter/linter, disassembler, trace/breakpoint interface and deterministic QVM mutation tests.

## Runtime and compute foundations

- Standalone typed SSA-like IR verifier and initial constant folding (not yet the default frontend lowering path).
- Bounded managed heap with explicit roots (QVM values have not yet migrated to it).
- Capability policy, bounded standard-library utilities, package-signature tooling, LSP diagnostics, Studio and Android JNI application foundations.
- Explicit native AI/GPU provider loaders. Provider modules are trusted native code supplied separately; no AI model runtime or vendor Vulkan/CUDA/Metal accelerator provider is bundled.
- Optional OpenCL provider (TAVIQ_ENABLE_OPENCL_PROVIDER=ON) executes real float32 vector addition on OpenCL GPU devices, with an OpenCL CPU fallback where a platform exposes no GPU device.

## Build

    cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
    cmake --build build --parallel
    ctest --test-dir build --output-on-failure

## Optional OpenCL provider

Install an OpenCL SDK/ICD (for example, ocl-icd-opencl-dev on Debian/Ubuntu), then:

    cmake -S . -B build-opencl -DCMAKE_BUILD_TYPE=Release -DTAVIQ_ENABLE_OPENCL_PROVIDER=ON
    cmake --build build-opencl --parallel
    ctest --test-dir build-opencl --output-on-failure
    build/taviq gpu-info build-opencl/taviq_opencl_provider.so
    build/taviq gpu-add build-opencl/taviq_opencl_provider.so 0 1,2,3 4,5,6

The provider is a native plugin and must be obtained from a trusted build. Its OpenCL dependency and device drivers must be installed on the host. A CPU fallback is clearly reported and is not GPU acceleration.

## Example

    fn factorial(n: number) -> number {
        if (n <= 1) {
            return 1;
        } else {
            return n * factorial(n - 1);
        }
    }

    flow main {
        display factorial(6);
    }

    build/taviq run examples/factorial.taviq
    build/taviq compile examples/factorial.taviq -o factorial.tvqbc
    build/taviq disasm factorial.tvqbc
    build/taviq run-bytecode factorial.tvqbc

## AI and GPU provider hosts

    build/taviq ai-generate /path/to/trusted-provider "Explain this function" 256 0.4
    build/taviq gpu-info /path/to/trusted-provider

These commands load trusted native provider libraries. The repository supplies versioned provider hosts, not bundled inference weights or an in-process AI inference engine.

See ROADMAP.md and docs/ for remaining work and explicit implementation boundaries.
