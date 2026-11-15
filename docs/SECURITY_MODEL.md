# Security Model

- Default-deny access to filesystem, network, process execution, and device resources.
- Capability grants are explicit and scoped.
- Package and model artifacts require integrity verification.
- Runtime resource limits cover memory, CPU time, threads, and file handles.
- Unsafe foreign-function calls are isolated behind explicit unsafe boundaries.
- Secrets must not be embedded in source code, logs, or package manifests.
- Security findings must be tracked; CI must not silently ignore failures.
