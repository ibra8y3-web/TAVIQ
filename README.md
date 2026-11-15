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
- Explicit native AI/GPU provider loaders. Provider modules are trusted native code supplied separately; no AI model runtime or production Vulkan/CUDA/Metal provider is bundled.

## Build

    cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
    cmake --build build --parallel
    ctest --test-dir build --output-on-failure

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

These commands load trusted native provider libraries. The repository supplies versioned provider hosts, not bundled inference weights, an inference engine, or vendor accelerator implementations.

See ROADMAP.md and docs/ for remaining work and explicit implementation boundaries.
