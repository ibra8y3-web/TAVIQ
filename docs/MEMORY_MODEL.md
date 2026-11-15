# TAVIQ Runtime Memory Model

## Current QVM v1

The implemented VM uses value-owned double and UTF-8 std::string values. Local variables are immutable let bindings, initialized exactly once by the compiler. Runtime values are released through C++ value lifetime; there is no garbage collector, pointer type, manual allocation API or user-visible ownership syntax yet.

The verified bytecode limits are:

- At most 1,000,000 instructions and 65,536 locals/operand-stack entries.
- At most 1 MiB for an individual encoded string or local identifier.
- At most 16 MiB for the aggregate encoded local-name and string-operand payload.
- At most 16 MiB of live string values represented by the verifier's straight-line stack/local analysis.
- One final HALT; no trailing bytes after a serialized program.
- Local slots may be initialized only once, preserving the current immutable let model.

The limit is a QVM format/runtime safety bound, not a promise that all host processes can allocate the maximum amount. Allocation failures remain host allocation failures and must be handled by applications.

## Deferred runtime design

A future managed runtime must specify object headers, allocation arenas, tracing or reference-counting policy, cycle behavior, deterministic finalization, thread ownership and FFI lifetime rules before exposing heap objects to user programs. No garbage collector is claimed by the current implementation.
