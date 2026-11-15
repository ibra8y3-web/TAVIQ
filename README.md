# TAVIQ — AI-Native Computing Language

**Beyond Code. Beyond Limits.**

TAVIQ is an open-source programming-language project for AI-oriented and high-performance computing. It is an evolving implementation; the repository distinguishes implemented subsets from planned production subsystems.

## Working implementation

- C++20 frontend for flow main, let, display, arithmetic, parentheses, strings and calls to len, sqrt and abs.
- Static type checks for supported values, duplicate/undefined names and intrinsic argument errors.
- QVM v1 bytecode serialization, disassembly, straight-line type/stack/local verification and bounded execution.
- C source lowering for the supported numeric subset and a host C compiler smoke test.
- CPU parallel-for with bounded chunk scheduling and exception propagation.
- Pinned Hugging Face model-asset installer, local llama.cpp HTTP client, SHA-256 checked package installer, local browser Studio and developer CLI tools.
- CI build/test jobs for Linux, Windows, macOS and Android arm64 cross-build.

The native backend emits C source; a host C compiler creates the machine executable. TAVIQ does not yet ship an LLVM backend, model inference library or GPU provider implementation. Downloading weights does not itself provide inference. Android CI is a cross-build, not physical-device runtime validation.

## Build

    cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
    cmake --build build --parallel
    ctest --test-dir build --output-on-failure

## Example

    flow main {
        let base = 2 + 3 * 4;
        let total = (base - 4) / 2;
        display base;
        display total * total;
        display len("TAVIQ");
        display sqrt(81) + abs(-3);
    }

    build/taviq run examples/calculator.taviq
    build/taviq compile examples/calculator.taviq -o calculator.tvqbc
    build/taviq disasm calculator.tvqbc
    build/taviq run-bytecode calculator.tvqbc
    build/taviq emit-c examples/calculator.taviq -o calculator.c

Windows multi-config builds usually place the executable at build/Release/taviq.exe.

## AI and model workflow

Review the model card and license first.

    python tools/taviq_hub.py org/model --accept-license --file model.gguf
    python ai/taviq_ai.py --prompt "Explain this function"

The Hub installs model assets at an immutable commit revision. An independently installed llama.cpp server (or another supported provider) is still required for inference.

## Packages, developer tools and Studio

    python tools/taviq_pkg.py list
    python tools/taviq_dev.py lint examples/hello.taviq --compiler build/taviq
    python tools/taviq_dev.py inspect examples/hello.taviq --compiler build/taviq
    python tools/taviq_dev.py profile examples/hello.taviq --compiler build/taviq --runs 20
    python studio/taviq_studio.py --root . --compiler build/taviq

See ROADMAP.md for stage-by-stage status and docs/PLATFORM_SUPPORT.md for platform evidence.
