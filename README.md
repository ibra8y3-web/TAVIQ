# TAVIQ — AI-Native Computing Language

**Beyond Code. Beyond Limits.**

TAVIQ is an open-source programming-language project for AI-oriented and high-performance computing. The repository reports implemented subsets and outstanding production work separately.

## Working implementation

- C++20 frontend with flow main, immutable let, mutable var, assignment, display, Boolean expressions, comparisons, if/else, while, typed functions, parameters and return values (non-recursive call-site expansion).
- Lexical block scopes, inferred or explicitly annotated number/string/bool declarations, and static type mismatch diagnostics.
- QVM v2 with conditional/unconditional jumps, CFG-aware verification, runtime instruction budgets and v1 bytecode loading compatibility.
- Deterministic QVM/lexer/parser mutation tests.
- C source lowering for the supported numeric subset; control flow is not yet lowered by the C backend.
- CPU parallel-for with bounded chunk scheduling and exception propagation.
- Explicit GPU provider ABI loader and gpu-info; production Vulkan/CUDA/Metal providers are not bundled.
- Pinned Hugging Face model-asset installer, local llama.cpp HTTP client, SHA-256 checked package installer, loopback browser Studio and developer CLI tools.
- CI builds and tests Linux, Windows and macOS; Android is NDK cross-build only.

## Build

    cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
    cmake --build build --parallel
    ctest --test-dir build --output-on-failure

## Language example

    flow main {
        var total: number = 0;
        var index: number = 0;

        while (index < 5) {
            index = index + 1;
            if (index == 3) {
                total = total + 10;
            } else {
                total = total + index;
            }
        }

        display total;
        display true and not false;
        display len("TAVIQ");
    }

    build/taviq run examples/calculator.taviq
    build/taviq compile examples/calculator.taviq -o calculator.tvqbc
    build/taviq disasm calculator.tvqbc
    build/taviq run-bytecode calculator.tvqbc

## Native output

    build/taviq emit-c examples/calculator.taviq -o calculator.c
    cc calculator.c -o calculator

The C emitter remains limited to supported numeric expressions and string literals; it rejects control-flow constructs it cannot lower.

## AI and model workflow

Review the model card and license first.

    python tools/taviq_hub.py org/model --accept-license --file model.gguf
    python ai/taviq_ai.py --prompt "Explain this function"

The Hub installs model assets at an immutable revision. An independently installed llama.cpp server (or another inference provider) is still required for actual inference.

## Engineering status

See ROADMAP.md, docs/LANGUAGE_SPEC.md, docs/VM_BYTECODE.md, docs/MEMORY_MODEL.md and docs/PLATFORM_SUPPORT.md. Native QVM call frames and recursion, modules, richer type-system features, LLVM, a native AI engine, production GPU backends, OS sandboxing, full standard library, public signed package registry, debugger/LSP and Android runtime testing remain outstanding.
