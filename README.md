# TAVIQ — AI-Native Computing Language

**Beyond Code. Beyond Limits.**

TAVIQ is an open-source programming-language project for AI-oriented and high-performance computing. This repository is an evolving implementation, not a claim that every planned subsystem is production-ready.

## What works

- C++20 frontend for flow main, let, numeric expressions, string literals and display.
- Checks for undefined/duplicate variables and invalid arithmetic types.
- QVM bytecode generation, versioned binary save/load, disassembly, static verification and bounded execution.
- Portable C source generation for the supported numeric subset.
- CPU parallel-for utility using standard C++ threads, including worker exception propagation.
- Python model installer with pinned Hugging Face revisions, file allow-list, license acknowledgement and SHA-256 checks.
- Local llama.cpp HTTP client, safe archive package manager and loopback browser Studio.
- CI builds Linux, Windows, macOS and cross-builds Android arm64 with the NDK.

The native C backend emits portable C source; a host C compiler turns it into a machine executable. The optional provider ABI is a contract, not a bundled inference engine. GPU provider loading and a Vulkan/CUDA backend are not yet implemented. Android CI is a cross-build, not a physical-device runtime test.

## Build

Requirements: CMake 3.20+, C++20 compiler, C compiler, and Python 3 for integration tests.

    cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
    cmake --build build --parallel
    ctest --test-dir build --output-on-failure

## Language example

    flow main {
        let base = 2 + 3 * 4;
        let total = (base - 4) / 2;
        display base;
        display total * total;
        display "Hello from TAVIQ";
    }

    build/taviq run examples/calculator.taviq
    build/taviq check examples/calculator.taviq
    build/taviq compile examples/calculator.taviq -o calculator.tvqbc
    build/taviq disasm calculator.tvqbc
    build/taviq run-bytecode calculator.tvqbc
    build/taviq emit-c examples/calculator.taviq -o calculator.c
    cc calculator.c -o calculator

Windows multi-config builds usually place the executable at build/Release/taviq.exe.

## AI model workflow

Review the model card and license first. The Hub installer resolves mutable refs to an immutable commit and installs selected model assets.

    python tools/taviq_hub.py org/model --accept-license --file model.gguf

Run a separately installed llama.cpp server bound to loopback, then call it.

    python ai/taviq_ai.py --prompt "Explain this function" --endpoint http://127.0.0.1:8080

Downloading model weights does not install or execute an inference engine. See docs/AI_HUB_SPEC.md.

## Packages and Studio

    python tools/taviq_pkg.py list
    python tools/taviq_pkg.py install sample --version 1.0.0 --index ./registry-index.json
    python studio/taviq_studio.py --root . --compiler build/taviq

The package manager requires a registry entry with an HTTPS archive URL and SHA-256 digest. It never executes package install scripts.

## Engineering status

See ROADMAP.md for implementation status and docs/PLATFORM_SUPPORT.md for verified versus planned targets. Full LLVM code generation, a separate optimized SSA IR, GPU provider implementation, full standard library, language-level imports/modules, complete debugger/profiler, and OS-level sandboxing remain future work.

## License

Apache-2.0. Third-party dependencies and model weights retain their own licenses.
