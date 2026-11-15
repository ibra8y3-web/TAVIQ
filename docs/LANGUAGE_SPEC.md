# TAVIQ Language Specification — Draft 0.2

## Identity and source form

TAVIQ source files use the .taviq extension; project configuration uses taviq.toml. A program has one entry block.

    program       = "flow" "main" "{" { statement } "}" ;
    statement     = declaration | display ;
    declaration   = "let" identifier "=" expression ";" ;
    display       = "display" expression ";" ;
    expression    = term { ("+" | "-") term } ;
    term          = unary { ("*" | "/") unary } ;
    unary         = "-" unary | primary ;
    primary       = number | string | identifier | "(" expression ")" ;

## Current semantics

- let declares an immutable local.
- Numbers are parsed as decimal floating-point values.
- Strings use double quotes and escapes for newline, tab, carriage return, quote and backslash.
- +, -, * and / operate on numbers. String concatenation with + is supported by QVM.
- display prints one value and a newline.
- Undefined names, duplicate declarations and invalid arithmetic types are rejected during compilation.
- Division by zero is a runtime error.

## Deferred language features

Functions, imports/modules, control flow, loops, pattern matching, generics, user-defined types, async tasks, language-level AI declarations, memory ownership and capability scopes are design targets, not part of the implemented grammar yet.

Identifiers in 0.2 are ASCII. Unicode identifiers require a separately specified normalization and anti-confusable policy.
