# TAVIQ Engineering Roadmap

Status reflects code and CI evidence on the default branch, not directory names or specifications.

| Stage | Scope | Current status | Remaining work |
|---|---|---|---|
| 01 | Foundation/build | implemented | signed reproducible releases |
| 02 | Frontend | implemented - expanded subset with typed non-recursive functions | native call frames/recursion, imports/modules, arrays, structs and Unicode identifiers |
| 03 | Type system | implemented - inferred/explicit primitives and typed function signatures | user-defined types, structs, generics, interfaces, union types and richer diagnostics |
| 04 | IR | implemented - QVM instruction IR | standalone SSA IR and optimization passes |
| 05 | QVM | implemented - v2 CFG verifier and v1 loader | coverage-guided fuzzing, source maps, calls/closures and richer opcodes |
| 06 | Native backend | implemented - C lowering subset | LLVM backend, optimization and control-flow lowering |
| 07 | Runtime | in progress | managed heap, GC/ownership, OS abstraction and structured concurrency |
| 08 | AI | in progress | bundled native inference engine, streaming, embeddings and model lifecycle API |
| 09 | Compute | implemented - provider host loader | production Vulkan/CUDA/Metal providers, scheduling and real-device validation |
| 10 | Security | in progress | OS sandbox, capability enforcement, signed providers and plugin isolation |
| 11 | Standard library | implemented - initial intrinsics | collections, IO, filesystem, networking, time, async and AI modules |
| 12 | Packages | implemented - local registry flow | public registry, signatures, dependency solving and publication |
| 13 | Developer tools | implemented - formatter/linter/inspector/basic profiler | LSP, debugger, breakpoints, source maps and sampling profiler |
| 14 | Platforms | implemented - CI builds | Android application/JNI layer and physical-device runtime validation |
| 15 | Studio | implemented - initial loopback editor | project model, language services, debugger integration and production UX |
| 16 | Release engineering | in progress | signed binaries, reproducible builds, SBOM and release promotion automation |

The current native C backend deliberately rejects if, else, while and user-defined functions; those features currently execute through QVM v2. Non-recursive functions are expanded at compile time, not invoked through native QVM call frames. The current AI client requires a separate inference service. GPU provider loading is implemented, but production accelerator modules are not bundled.
