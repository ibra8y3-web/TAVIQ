# TAVIQ Engineering Roadmap

Status is based on code and automated checks in this repository, not on directory names or design documents.

| Stage | Scope | Current status | Evidence / remaining work |
|---|---|---|---|
| 01 | Foundation and build system | implemented | CMake, CI, license, repository policy |
| 02 | Lexer, parser, AST | implemented - subset | flow main, let, expressions, strings, diagnostics; functions/control flow/modules remain |
| 03 | Type checking | implemented - subset | numeric/string expression checks, duplicate and undefined-name checks; full type system/generics remain |
| 04 | Intermediate representation | implemented - QVM instruction IR | AST lowers to typed instruction sequence; separate SSA IR and optimization passes remain |
| 05 | QVM bytecode/runtime | implemented - v1 | versioned binary format, serialization, disassembly, stack/type/local verifier and bounded execution; fuzzing remains |
| 06 | Native backend | implemented - C lowering subset | emits portable C for numeric expressions and string literals; LLVM backend and broader lowering remain |
| 07 | Core runtime | in progress | VM values, locals, stack/instruction limits; memory manager, GC/ownership and OS abstractions remain |
| 08 | AI engine | in progress | llama.cpp loopback HTTP client and provider ABI; native provider loader, streaming and embeddings remain |
| 09 | Compute engine | in progress | CPU parallel-for; SIMD and actual Vulkan/CUDA/Metal providers remain |
| 10 | Security | in progress | verified QVM, safe model/package handling, loopback Studio; OS sandbox and capability enforcement remain |
| 11 | Standard library | not started | language-level modules and stable APIs remain |
| 12 | Package ecosystem | implemented - local registry flow | HTTPS index, pinned SHA-256 archives, safe extraction, lock file; public registry/signatures remain |
| 13 | Developer tools | in progress | CLI run/check/compile/disasm, package and model tools; formatter, debugger and profiler remain |
| 14 | Cross-platform | in progress | CI builds Linux/Windows/macOS and cross-builds Android arm64; physical-device runtime validation remains |
| 15 | TAVIQ Studio | implemented - initial local editor | loopback browser editor, save/run; language services, breakpoints and project explorer remain |
| 16 | Release engineering | in progress | CI test matrix and artifacts; signed release process and reproducible builds remain |

A stage must not be marked complete merely because its directory or specification exists. Hardware-dependent features require provider implementation and real-device testing before they can be promoted.
