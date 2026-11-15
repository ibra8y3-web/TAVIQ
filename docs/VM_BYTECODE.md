# TAVIQ QVM Bytecode v1

## Container format

The binary begins with the eight-byte signature TVQVM01 followed by little-endian 32-bit instruction and local-table counts. Local names are length-prefixed UTF-8 strings. Each instruction stores an opcode byte, a 32-bit local slot, a 64-bit IEEE-754 number field, and a length-prefixed UTF-8 text field. Unused operands are serialized as zero/empty values.

The loader rejects unknown versions/opcodes, excessive section counts, oversized strings and truncated sections.

## Initial instruction set

PUSH_NUMBER, PUSH_STRING, LOAD_LOCAL, STORE_LOCAL, ADD, SUB, MUL, DIV, NEG, PRINT, HALT.

The command taviq compile input.taviq -o output.tvqbc emits the container. taviq run-bytecode loads and executes it. taviq disasm prints a human-readable listing.

## Safety

The VM has an instruction budget, stack limit, local-slot validation, initialization checks, division-by-zero checks and output-stream error handling. A separate static bytecode verifier, fuzzing campaign and OS sandbox are still required before treating bytecode as safe for hostile multi-tenant execution.
