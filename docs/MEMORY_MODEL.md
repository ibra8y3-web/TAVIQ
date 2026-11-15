# TAVIQ Memory Model — Target Design

The current prototype stores runtime values in standard C++ vectors and variants. This file describes the direction for a future language-level memory model, not shipped functionality.

Target requirements:
- explicit ownership and borrowing rules for mutable data;
- deterministic destruction and well-defined move semantics;
- arenas/pools for compiler and runtime allocations;
- bounded allocation quotas for sandboxed execution;
- safe FFI ownership transfer and ABI-stable handles;
- tests for aliasing, lifetime errors, leaks and allocation failure.

A garbage collector or borrow checker must not be claimed until implemented and tested.
