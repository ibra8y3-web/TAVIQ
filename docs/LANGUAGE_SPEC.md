# TAVIQ Language Specification — Draft 0.3

## Source form

    program       = "flow" "main" "{" { statement } "}" ;
    statement     = declaration | display ;
    declaration   = "let" identifier "=" expression ";" ;
    display       = "display" expression ";" ;
    expression    = term { ("+" | "-") term } ;
    term          = unary { ("*" | "/") unary } ;
    unary         = "-" unary | primary ;
    primary       = number | string | identifier [ "(" [ arguments ] ")" ] | "(" expression ")" ;
    arguments     = expression { "," expression } ;

## Implemented semantics

- let declares an immutable local.
- Numbers are decimal floating-point values; strings use double quotes and common escapes.
- Numeric +, -, * and / are supported. String concatenation with + is supported in QVM.
- display prints one value and a newline.
- len(string) returns UTF-8 byte length; sqrt(number) returns square root for non-negative input; abs(number) returns absolute value. Each accepts exactly one argument.
- Undefined names, duplicate declarations and invalid built-in argument types are rejected during compilation.
- Division by zero and invalid square-root inputs are runtime errors.

## Deferred features

User-defined functions, imports/modules, control flow, loops, pattern matching, generics, user-defined types, async tasks, language-level AI declarations, memory ownership and capability scopes are not implemented in this grammar yet.

Identifiers are ASCII in version 0.3. Unicode identifiers require normalization and anti-confusable rules.
