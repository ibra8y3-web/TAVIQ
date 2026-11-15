# TAVIQ Engineering Roadmap

Status reflects the code merged into the default branch and completed CI evidence, not directory names or design documents.

| Stage | Scope | Current status | Remaining work |
|---|---|---|---|
| 01 | Foundation/build | implemented | signed reproducible releases |
| 02 | Frontend | implemented - subset | user functions, conditionals, loops, modules and Unicode identifiers |
| 03 | Type system | implemented - subset | booleans, user types, generics, inference and richer diagnostics |
| 04 | IR | implemented - QVM instruction IR | standalone SSA IR and optimization passes |
| 05 | QVM | implemented - v1 verified subset | fuzzing, broader instruction set and verified control-flow graphs |
| 06 | Native backend | implemented - C lowering subset | LLVM backend and broader language lowering |
| 07 | Runtime | in progress | managed memory, ownership model, OS abstraction and structured concurrency |
| 08 | AI | in progress | native provider loader, streamed tokens, embeddings and model lifecycle APIs |
| 09 | Compute | in progress | SIMD plus real Vulkan/CUDA/Metal providers and device validation |
| 10 | Security | in progress | OS sandbox, capability enforcement and hardened plugin isolation |
| 11 | Standard library | implemented - initial intrinsics | collections, IO, filesystem, networking, time, async and AI modules |
| 12 | Packages | implemented - local registry flow | public registry, signatures, dependency solving and lockfile verification |
| 13 | Developer tools | implemented - initial CLI set | LSP, full debugger, source maps and sampling profiler |
| 14 | Platforms | implemented - CI builds | Android runtime/device validation and platform-specific packaging |
| 15 | Studio | implemented - initial loopback editor | language services, project model, breakpoints and integrated debugger |
| 16 | Release engineering | in progress | signed binaries, reproducible builds, SBOM and automated release promotion |

The QVM verifier enforces stack typing, one-time local initialization, encoded-string bounds and a 16 MiB live-string budget. It is still a straight-line bytecode verifier; control-flow instructions must not be added without a control-flow-aware dataflow verifier.
