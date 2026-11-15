# TAVIQ — AI-Native Computing Language

TAVIQ is an evolving open-source programming-language and compute-runtime project.

## Implemented core

- C++20 frontend for flow main, let, var, explicit number/string/bool annotations, expressions, comparisons, if/else and while.
- QVM v2 with typed CFG verification, runtime instruction budgets, bounded strings and v1 bytecode loading.
- Portable C source emitter for its documented subset.
- CPU parallel-for and explicit GPU provider ABI host.
- Native AI provider ABI loader with bounded prompts/responses. A provider library must be supplied by the user and is trusted native code; TAVIQ does not ship inference weights or an inference engine.
- Standalone typed SSA IR model, verifier and initial constant-folding optimizer. The frontend still lowers directly to QVM; the SSA pipeline is not yet the default compiler path.
- Standalone mark-and-sweep managed heap with explicit root handles. The current QVM value storage has not yet migrated to the managed heap.
- QVM mutation/fuzz tests, CLI/Studio and Python security tooling.

## Build

    cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
    cmake --build build --parallel
    ctest --test-dir build --output-on-failure

## Native AI provider

    build/taviq ai-generate /path/to/trusted-provider.so "Explain this algorithm" 256 0.4

On Windows, provide a trusted DLL; on macOS a trusted dylib. Provider ABI is defined in include/taviq/ai_provider.h.

## GPU providers

    build/taviq gpu-info /path/to/trusted-gpu-provider

The provider host is implemented, but production Vulkan/CUDA/Metal providers are not bundled.

## Android

The Android application module and JNI bridge are being added separately under platforms/android. NDK cross-build success is not a physical-device runtime test.

See ROADMAP.md and docs/ for exact subsystem status and limitations.
