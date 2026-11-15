# Memory Model — Design Draft

The target model combines statically checked ownership/borrowing for native values
with managed handles for runtime-managed objects such as model sessions and tensors.
The language must define aliasing, mutability, lifetimes, thread transfer, pinning,
and FFI rules before the model is considered stable. No claim of memory safety is
made by the current display-only interpreter.
