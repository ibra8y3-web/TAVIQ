# QVM Bytecode Format v1

QVM v1 is a little-endian, version-tagged bytecode container. Its magic is TVQVM01 followed by a zero byte.

## Container layout

1. 8-byte magic/version.
2. 32-bit instruction count.
3. 32-bit local-name count.
4. Each local name: 32-bit byte length followed by UTF-8 bytes.
5. Each instruction: 8-bit opcode, 32-bit slot, 64-bit IEEE-754 operand bits, 32-bit text length and text bytes.

## Supported opcodes

PUSH_NUMBER, PUSH_STRING, LOAD_LOCAL, STORE_LOCAL, ADD, SUB, MUL, DIV, NEG, PRINT, HALT, LENGTH, SQRT and ABS.

## Verification

The loader, serializer and VM entry point call the same verifier. It rejects unknown opcodes, non-finite constants, type-invalid operations, stack underflow, invalid/uninitialized locals, duplicate local initialization, non-final HALT, non-empty stack at HALT, oversized encoded payloads and live string usage above 16 MiB. Serialized input with bytes after the final instruction is rejected.

The verifier assumes straight-line bytecode. Control flow or function-call opcodes require a control-flow graph verifier with dataflow joins; do not extend the instruction set with jumps before that exists.

## Limits

- 1,000,000 instructions.
- 65,536 locals and operand-stack entries.
- 1 MiB per encoded string.
- 16 MiB aggregate encoded strings.
- 16 MiB live string memory estimated for the straight-line program.
