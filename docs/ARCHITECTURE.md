# TAVIQ Architecture

## Intended pipeline
Source -> Lexer -> Parser -> AST -> Semantic Analysis -> TIR -> Optimizer ->
(QVM bytecode | Native backend) -> Runtime.

## Current implementation
The repository currently implements a small lexer, parser, AST statement list,
and interpreter for `flow main { display "text"; }`.

## Planned components
- Typed TIR with explicit control-flow and data-flow representation.
- Verifier before execution.
- QVM bytecode format with versioning and validation.
- LLVM-based native backend behind a backend interface.
- Platform abstraction layer for OS-specific APIs.
- Stable C ABI for foreign-function integration.

## Design constraints
Compiler frontends must not depend on AI providers. Runtime AI integration must be
optional and capability-controlled. The core compiler must build without GPU SDKs
or network access.
