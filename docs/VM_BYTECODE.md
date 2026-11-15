# QVM Bytecode Formats

## Compatibility

- QVM v1 (TVQVM01) remains readable.
- QVM v2 (TVQVM02) adds Boolean values, comparisons, mutable locals and control-flow jumps.
- QVM v3 (TVQVM03) adds function metadata, CALL and RETURN. Newly compiled source uses v3.

## Container layout

V1 and v2 retain their original layout: magic, instruction count, local-name count, local names, then instructions containing opcode, slot, IEEE-754 operand bits and text.

V3 stores:
1. Magic, instruction count, main-local count, function count and main-code end offset.
2. Main local names.
3. Each function: name, absolute entry/end offsets, return type, parameter type list, local count and local names.
4. Instructions: opcode, slot/target/function index, argument count, IEEE-754 operand bits and text.

All integer and floating-point bit fields are little-endian.

## V3 operations

- CALL stores the function-table index in slot and parameter count in argumentCount.
- CALL pops arguments in reverse order into a newly allocated frame.
- RETURN pops exactly one value of the declared function return type and transfers it to the caller.
- Every call has isolated locals, operand-stack base and return address. Recursion is supported with a runtime call-depth limit.
- Main ends with HALT. Function ranges end with RETURN. Jumps cannot leave their current code range.

## Verification

The serializer, loader and VM entry point share one verifier. V1/v2 retain compatibility checks. V3 validates function ranges, signatures, argument count/type, stack merges, local definite initialization/mutability, return values, control-flow targets and reachable terminal instructions. Each function is data-flow verified separately with parameters initialized at entry.

The runtime independently enforces instruction, stack, memory, active-local and call-depth limits. Malformed call targets, argument lists, function ranges, recursive stack exhaustion and type-invalid returns are rejected.

## Limits

- 1,000,000 instructions.
- 65,536 locals per frame and operand-stack values.
- 1 MiB per encoded string; 16 MiB aggregate encoded strings.
- 16 MiB estimated live runtime strings.
- 256 nested function frames and 1,000,000 active local slots.
