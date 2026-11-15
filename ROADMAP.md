# TAVIQ Engineering Roadmap

Status reflects code merged into the default branch and completed CI evidence, not directory names or specifications.

| Stage | Scope | Current status | Remaining work |
|---|---|---|---|
| 01 | Foundation/build | implemented | signed reproducible releases |
| 02 | Frontend | implemented - subset | functions, conditions, loops, modules, Unicode identifiers |
| 03 | Type system | implemented - subset | booleans, user types, generics and richer inference |
| 04 | IR | implemented - QVM instruction IR | SSA IR and optimization passes |
| 05 | QVM | implemented - v1 verified subset | fuzzing and control-flow-aware verification |
| 06 | Native backend | implemented - C lowering subset | LLVM backend and broader lowering |
| 07 | Runtime | in progress | managed memory, ownership, OS abstraction and concurrency |
| 08 | AI | in progress | native inference/provider loader, streaming, embeddings |
| 09 | Compute | in progress | GPU provider host ABI/loader implemented; production Vulkan/CUDA/Metal providers and device tests remain |
| 10 | Security | in progress | OS sandbox, capability enforcement and plugin signatures/isolation |
| 11 | Standard library | implemented - initial intrinsics | collections, IO, filesystem, networking, time, async and AI modules |
| 12 | Packages | implemented - local registry flow | public registry, signatures and dependency solving |
| 13 | Developer tools | implemented - initial CLI set | LSP, debugger, source maps and sampling profiler |
| 14 | Platforms | implemented - CI builds | Android app wrapper and physical-device runtime validation |
| 15 | Studio | implemented - initial loopback editor | language services, project model and integrated debugger |
| 16 | Release engineering | in progress | signed binaries, reproducible builds, SBOM and promotion automation |

Provider loading executes native code from the explicitly named library. Only load trusted providers. The loader is an ABI host, not a bundled GPU implementation.
