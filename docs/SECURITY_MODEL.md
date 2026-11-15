# TAVIQ Security Model

## Current safeguards

- QVM v1/v2/v3 loading validates version, opcode ranges, instruction/function ranges, stack/type flow, local initialization, call signatures and terminal instructions before execution.
- Runtime enforces instruction, stack, live-string, active-local and recursive call-depth limits.
- AI Hub pins model assets to an immutable repository revision, requires explicit license acknowledgement, restricts file formats and verifies SHA-256 when Hub LFS metadata supplies it.
- Package installation uses HTTPS, checks archive SHA-256, supports optional Ed25519-signed registry-index verification, rejects traversal and archive links/special files, limits extraction size and never executes install scripts.
- Studio binds to loopback, restricts browser origins, bounds request sizes and invokes subprocesses without a shell.
- Capability primitives define file/network/process/provider permission flags, workspace path checks and instruction budgets.
- \`tools/taviq_sandbox.py\` is a fail-closed Linux Bubblewrap launcher. It refuses to run when Bubblewrap is unavailable, creates namespaces, denies networking by default and supports a read-only workspace mode.

## Explicit limitations

The capability policy is not yet wired into every QVM operation. Bubblewrap is an external Linux process launcher, not a portable VM sandbox. There are no equivalent Windows AppContainer, macOS Seatbelt or Android isolated-process adapters in this release. Path-based checks cannot by themselves eliminate every filesystem TOCTOU race.

Native C output and AI/GPU provider plugins execute native code with the caller's privileges. Provider libraries must be trusted and separately reviewed; the dynamic ABI does not isolate them. Model parsing still belongs in a hardened inference runtime.

Do not run untrusted native output or provider plugins. Do not expose Studio or a local llama.cpp endpoint to untrusted networks. OS sandbox integration, capability enforcement throughout the VM, signed provider provenance and broader adversarial fuzzing remain outstanding.
