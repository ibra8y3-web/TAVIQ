# TAVIQ Runtime Memory Model

## QVM frames

QVM v3 stores runtime values as value-owned \`double\`, UTF-8 \`std::string\` and Boolean values. The main flow has its own locals; each function call allocates a separate frame with parameter/local slots, initialization and mutability state, operand-stack base and return address. Recursive calls are supported with a maximum call-depth limit. Returning destroys the callee frame and transfers one return value to the caller.

\`let\` slots are immutable after initialization. \`var\` slots may be reassigned without changing type. QVM v1/v2 bytecode remains readable; new source is compiled to QVM v3.

## Managed heap component

\`include/taviq/heap.hpp\` and \`memory/heap.cpp\` provide a separate bounded object heap with String/Array/Record objects, explicit RAII root handles and mark/sweep collection. It validates references, tracks configured byte/object limits and exposes inspection/collection APIs.

**Integration boundary:** QVM's current number/string/Boolean values and function frames do not yet use this managed heap. Source-language arrays/records, object references, ownership/borrowing and automatic GC integration are not implemented. The heap is a reusable runtime component, not a claim that all TAVIQ allocations are garbage-collected.

## Runtime limits

- QVM instruction budget defaults to 1,000,000 executed instructions.
- Operand stack is bounded to 65,536 values.
- Function recursion is limited to 256 active frames.
- Active function/main local slots are bounded to 1,000,000 total.
- Estimated live QVM strings are bounded to 16 MiB.
- Encoded bytecode strings are limited to 1 MiB each and 16 MiB in aggregate.

These limits are protective ceilings, not a promise that every host can allocate the maximum. Allocation failures remain possible and are surfaced as runtime errors where handled.
