# Memory model

The current VM uses scoped C++ value containers for its stack and local slots. It does not yet implement a language-level ownership model, garbage collector, arena allocator, borrow checker or explicit memory API. See docs/MEMORY_MODEL.md for the target design.
