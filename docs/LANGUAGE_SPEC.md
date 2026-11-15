# TAVIQ Language Specification — Draft 0.6

## Program form

    program       = "flow" "main" block ;
    block         = "{" { statement } "}" ;
    function      = "fn" identifier "(" [ parameters ] ")" "->" type_name block ;
    parameters    = parameter { "," parameter } ;
    parameter     = identifier ":" type_name ;
    statement     = declaration | assignment | display | if_stmt | while_stmt | return ;
    return        = "return" expression ";" ;
    declaration   = ("let" | "var") identifier [ ":" type_name ] "=" expression ";" ;
    type_name     = "number" | "string" | "bool" ;
    assignment    = identifier "=" expression ";" ;
    display       = "display" expression ";" ;
    if_stmt       = "if" "(" expression ")" block [ "else" block ] ;
    while_stmt    = "while" "(" expression ")" block ;

Function declarations appear before `flow main`. Parameters and return types must be `number`, `string` or `bool`. Function bodies must return a value on every path. Non-recursive calls and calls between functions are expanded into QVM v2 at compile time; recursion is rejected until the runtime has native call frames. Locals and parameters are reset at each expanded call, including when the call occurs in a loop.

## Expression precedence (highest to lowest)

    primary       = number | string | "true" | "false"
                  | identifier [ "(" [ arguments ] ")" ]
                  | "(" expression ")" ;
    unary         = ("-" | "!" | "not") unary | primary ;
    factor        = unary { ("*" | "/") unary } ;
    term          = factor { ("+" | "-") factor } ;
    comparison    = term { ("<" | "<=" | ">" | ">=") term } ;
    equality      = comparison { ("==" | "!=") comparison } ;
    logical_and   = equality { "and" equality } ;
    logical_or    = logical_and { "or" logical_and } ;
    expression    = logical_or ;
    arguments     = expression { "," expression } ;

## Implemented semantics

- let creates an immutable local. A second assignment to that name is a compile-time error.
- var creates a mutable local. Assignments must preserve its inferred type.
- An optional `: number`, `: string` or `: bool` annotation makes the expected declaration type explicit; the initializer must match it. Declarations without annotations continue to infer their type.
- Braced blocks create lexical scopes. Locals declared in a branch do not escape that branch.
- if and optional else execute one branch based on a Boolean condition.
- while repeats while its Boolean condition is true. The VM counts executed instructions against its runtime budget.
- true, false, not/! , and and or are Boolean operations. and and or short-circuit.
- == and != compare values of the same type. Ordering comparisons currently accept numbers only.
- Arithmetic supports numeric +, -, *, /; QVM also supports string concatenation with +.
- display prints numbers, strings and Boolean values followed by a newline.
- len(string) returns UTF-8 byte length; sqrt(number) accepts non-negative values; abs(number) returns absolute value.
- Undefined names, duplicate declarations in a scope, invalid assignments and non-Boolean branch/loop conditions are rejected by the compiler.

## Current limitations

- Explicit annotations currently cover only number, string and bool. User-defined types, unions, generics and interfaces are not implemented.
- Functions currently use static call-site expansion. Recursion, first-class function values, closures and native QVM call frames are not implemented.
- Imports/modules, arrays, structs, pattern matching, async tasks and language-level AI declarations are not implemented yet.
- To keep loop-local storage safe with the current local-slot model, declarations inside a while body must use var; let declarations inside loops are rejected.
- The current portable C emitter remains a numeric subset and does not yet lower if, else or while.
- Identifiers are ASCII in this version. Unicode identifiers require normalization and anti-confusable handling before they can be enabled safely.
