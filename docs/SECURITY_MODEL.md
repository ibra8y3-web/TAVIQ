# TAVIQ Security Model

## Current safeguards

- The QVM checks local-slot bounds, uninitialized locals, stack underflow, instruction budgets and arithmetic errors.
- The QVM file reader validates its version, opcode range, section counts and string sizes before allocation.
- AI model installation resolves a repository ref to an immutable commit, restricts file formats, requires a user license acknowledgement and verifies SHA-256 when Hub LFS metadata supplies it.
- Package installation requires HTTPS, a registry-provided SHA-256, rejects traversal paths and links/special archive members, limits archive/unpacked sizes, and never runs install scripts.
- Studio binds only to 127.0.0.1, rejects foreign browser origins, limits request sizes and uses subprocess argument arrays rather than a shell.

## Explicit limitations

The VM is not a process sandbox. No OS-level seccomp, AppContainer, Seatbelt or Android isolated-process policy is applied. Native C output executes with the caller's privileges. Third-party model runtimes and provider plugins are native code and must be trusted and separately reviewed. The Python model downloader does not execute repository scripts, but model parsing still belongs in a hardened inference runtime.

Do not run untrusted native output or plugins. Do not expose the local Studio or llama.cpp endpoint to untrusted networks. A future release must add an independent bytecode verifier, fuzzing, OS sandbox integration, signed package metadata and supply-chain pinning.
