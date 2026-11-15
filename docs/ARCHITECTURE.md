# TAVIQ Architecture

## Implemented execution paths

Source -> Lexer -> Parser -> AST -> type checks during lowering -> QVM instruction stream -> bounded VM execution.

Source -> Lexer -> Parser -> AST -> native C lowering -> host C compiler -> machine executable.

The C backend is intentionally described as C lowering; the compiler does not currently link LLVM or emit LLVM IR.

## Runtime boundaries

The QVM uses a versioned binary bytecode container. It checks format version, opcode range, section sizes, local-slot bounds, stack underflow and an instruction budget. Bytecode from untrusted sources still requires process isolation for hostile workloads; the VM is not an OS sandbox.

The CPU compute helper offers a bounded, exception-propagating parallel-for utility. GPU compute is exposed as a future provider boundary; no Vulkan/CUDA/Metal execution plugin ships in this version.

## AI integration

The AI Hub is a Python asset installer, not an inference runtime. It pins model refs to a Hub commit and verifies LFS SHA-256 where supplied. ai/taviq_ai.py talks to a separately launched llama.cpp server over loopback HTTP. A versioned C provider ABI is defined for future native plugins but is not yet loaded by the language runtime.

## Platform strategy

Core language code uses C++20 and standard threads. Host CI builds Linux, Windows and macOS. An Android arm64 NDK cross-build is included; Android runtime APIs and device execution are not yet certified.
