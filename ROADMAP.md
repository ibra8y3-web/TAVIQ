# TAVIQ Engineering Roadmap

Status reflects committed implementation and CI evidence, not documentation directories.

| Stage | Scope | Status after QVM v3 implementation | Remaining |
|---|---|---|---|
| 01 | Foundation/build | implemented | tagged release artifacts |
| 02 | Frontend | typed functions, parameters, returns, recursion, if/else and while | modules/imports, arrays, structs, closures and Unicode identifiers |
| 03 | Type system | inferred/explicit number/string/bool and typed signatures | user types, generics, interfaces, unions and richer inference |
| 04 | IR | standalone typed SSA-like IR and initial constant folding | complete Phi/dominance and frontend-to-SSA lowering |
| 05 | QVM | v3 CALL/RETURN frames, recursion, v1/v2 compatibility and CFG verification | closures, source maps, advanced fuzzing and richer opcodes |
| 06 | Native backend | C lowering for supported scalar and structured control flow | function/aggregate lowering and LLVM/JIT |
| 07 | Runtime | bounded managed heap with explicit roots and mark/sweep | connect heap to QVM values, ownership, FFI and OS abstractions |
| 08 | AI | native provider ABI host and CLI | bundled inference engine, streaming, embeddings and lifecycle |
| 09 | Compute | dynamic GPU provider host | production Vulkan/CUDA/Metal modules and physical-device validation |
| 10 | Security | capability policy and fail-closed Linux Bubblewrap launcher | VM integration and Windows/macOS/Android sandbox adapters |
| 11 | Standard library | bounded arrays, workspace file IO, UTC time, binary serialization, plain TCP | TLS, JSON, async and broader collections/platform APIs |
| 12 | Packages | semver resolver, signed index publishing and local registry | automatic transitive installation and hosted service |
| 13 | Developer tools | formatter, linter, inspector, profiler, trace breakpoints and initial LSP | full semantic LSP and interactive source debugger |
| 14 | Platforms | Android Gradle/JNI app and emulator tests | physical device testing, lifecycle/storage polish |
| 15 | Studio | loopback editor, project actions and diagnostics | full LSP/debugger UI integration |
| 16 | Release | reproducible archives, CycloneDX source inventory and Sigstore workflow | signed tag releases and complete dependency SBOM |

Unsupported native constructs and provider backends fail explicitly rather than silently omitting work.
