# TAVIQ Architecture

## Implemented execution paths

Source -> Lexer -> Parser -> AST -> type checks during lowering -> QVM instruction stream -> bounded VM execution.

Source -> Lexer -> Parser -> AST -> native C lowering -> host C compiler -> machine executable.

The native C backend supports typed scalar user functions, parameters, function calls, recursion, return values and structured control flow. Aggregate types and string concatenation remain unsupported by this backend. The C backend does not currently link LLVM or emit LLVM IR.

## Runtime boundaries

The QVM uses a versioned binary bytecode container. It checks format version, opcode range, section sizes, local-slot bounds, stack underflow and an instruction budget. Bytecode from untrusted sources still requires process isolation for hostile workloads; the VM is not an OS sandbox.

The CPU compute helper offers a bounded, exception-propagating parallel-for utility. The GPU host loads a versioned native provider. An optional OpenCL module implements float32 vector addition using OpenCL GPU devices where available and an explicitly labelled OpenCL CPU fallback otherwise. Vulkan, CUDA and Metal providers are not bundled.

## Package installation boundary

The package manager solves semantic-version constraints for the transitive package graph, stages all required HTTPS archives, verifies SHA-256 values, rejects unsafe archive paths and links, and only replaces installed package trees after every artifact has been verified and extracted. Directory changes are rolled back if the commit step fails, and the lockfile is atomically replaced. Package-provided installation scripts are never executed. Registry publishing and a hosted discovery service are separate future components.

## AI integration

The AI Hub is a Python asset installer, not an inference runtime. It pins model refs to a Hub commit and verifies LFS SHA-256 where supplied. ai/taviq_ai.py talks to a separately launched llama.cpp server over loopback HTTP. The native provider host defines a trusted extension point but no in-process inference engine is bundled.

## Platform strategy

Core language code uses C++20 and standard threads. Host CI builds Linux, Windows and macOS. An Android arm64 NDK cross-build and Android application workflow are included; Android runtime APIs and execution on a physical device are not certified. The optional OpenCL provider is only built on hosts with an installed OpenCL SDK and driver.
