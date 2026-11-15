# TAVIQ Runtime Memory Model

## QVM v2 values

The VM currently stores value-owned double, UTF-8 std::string and Boolean values. let locals are immutable; var locals use a mutable slot. Local values are released through C++ value lifetime. There is no user-visible pointer type, manual allocator or garbage collector yet.

## Verifier memory model

The verifier performs control-flow dataflow over the QVM v2 instruction graph. Each abstract state tracks operand stack types, local initialization/mutability and string byte counts. At branch merges it requires compatible stack shapes and conservatively combines local and string states. Unbounded or varying loop-carried strings are rejected rather than assumed safe.

Limits:
- 1,000,000 instructions and 65,536 locals/operand-stack entries.
- 1 MiB per encoded string or local identifier.
- 16 MiB aggregate encoded names and instruction strings.
- 16 MiB estimated live string values including locals and stack copies.
- Runtime instruction budget counts executed steps, so an infinite loop terminates with a budget error.

These limits are verifier/runtime bounds, not a promise that every host can allocate the maximum. Allocation failures remain possible and must be handled by embedding applications.

## Deferred memory design

A future managed runtime must specify object headers, allocation arenas, tracing or reference-counting policy, cycle behavior, deterministic finalization, thread ownership and FFI lifetimes before exposing heap objects to user programs. No garbage collector is claimed by the current implementation.
