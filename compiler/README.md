# Compiler frontend

lexer.cpp tokenizes the current language subset; parser.cpp creates the source AST; compiler.cpp performs basic type/name checks and lowers supported expressions into QVM instructions. native.cpp emits portable C for numeric declarations/expressions and string-literal display. Functions, flow control, modules, optimization and LLVM lowering remain future work.
