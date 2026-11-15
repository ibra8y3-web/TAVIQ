# QVM Bytecode Formats

## Version compatibility

- QVM v1 uses the magic bytes TVQVM01 followed by a zero byte. It is still accepted by the loader and VM.
- QVM v2 uses TVQVM02 followed by a zero byte. Newly compiled source is emitted as v2.
- Both versions use the same little-endian container sections. V1 is restricted to opcodes 1–14; v2 adds Boolean and control-flow operations.

## Container layout

1. 8-byte magic/version.
2. 32-bit instruction count.
3. 32-bit local-name count.
4. Each local name: 32-bit byte length and the encoded name bytes.
5. Each instruction: 8-bit opcode, 32-bit slot/target, 64-bit IEEE-754 operand bits, 32-bit text length and text bytes.

## V2 additions

- PUSH_BOOLEAN, EQUAL, NOT_EQUAL, LESS, LESS_EQUAL, GREATER, GREATER_EQUAL and LOGICAL_NOT.
- JUMP and JUMP_IF_FALSE use an absolute instruction index in the slot field. Conditional jumps pop one Boolean.
- STORE_LOCAL_MUTABLE is used for var; STORE_LOCAL remains one-time immutable initialization.
- Existing arithmetic, string, print and built-in opcodes retain their v1 numeric values.

## Verification

The loader, serializer and VM entry point call the same verifier. V1 is checked as straight-line bytecode. V2 uses a control-flow worklist and validates every encoded instruction, jump target, stack height/type at merge points, local definite initialization, local mutability, type consistency, and final reachable HALT.

The verifier rejects invalid opcodes, non-finite constants, type-invalid operations, stack underflow, path-dependent uninitialized loads, repeated immutable stores, mutable type changes, malformed control flow, invalid jump targets, non-final HALT, trailing bytes, oversized encoded strings and live string usage above 16 MiB.

A loop that grows a carried string without a statically stable bounded size is rejected conservatively. Runtime execution also enforces an instruction budget, counting executed instructions rather than only bytecode length.

## Limits

- 1,000,000 encoded instructions.
- 65,536 locals and operand-stack entries.
- 1 MiB per encoded string.
- 16 MiB aggregate encoded local names and instruction strings.
- 16 MiB estimated live string values.
