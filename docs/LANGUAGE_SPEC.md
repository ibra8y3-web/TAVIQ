# TAVIQ Language Specification — Draft 0.6

## Program form

    program       = { function_decl } "flow" "main" block ;
    function_decl = "fn" identifier "(" [ parameters ] ")" "->" type_name block ;
    parameters    = parameter { "," parameter } ;
    parameter     = identifier ":" type_name ;
    type_name     = "number" | "string" | "bool" ;
    block         = "{" { statement } "}" ;
    statement     = declaration | assignment | display | if_stmt | while_stmt | return_stmt ;
    declaration   = ("let" | "var") identifier [ ":" type_name ] "=" expression ";" ;
    assignment    = identifier "=" expression ";" ;
    display       = "display" expression ";" ;
    return_stmt   = "return" expression ";" ;
    if_stmt       = "if" "(" expression ")" block [ "else" block ] ;
    while_stmt    = "while" "(" expression ")" block ;

## Functions and calls

    fn add(a: number, b: number) -> number {
        return a + b;
    }

    flow main {
        display add(20, 22);
    }

Function declarations appear before flow main. Parameters and return values require explicit number, string or bool types. Calls are expressions and can be nested or recursive. The compiler rejects unknown functions, duplicate function or parameter names, wrong argument count/types, mismatched return values and functions that do not return on every possible path.

Each call creates a separate QVM frame with its own local slots, operand-stack base and return address. A return statement transfers exactly one typed value to its caller. Runtime limits cap recursive call depth and active local slots.

## Expressions and built-ins

Expression precedence is unary, multiplication/division, addition/subtraction, comparisons, equality, logical AND, then logical OR. AND and OR short-circuit. Equality compares values of the same type; ordering currently supports numbers only.

Built-ins:
- len(string) returns UTF-8 byte length.
- sqrt(number) accepts non-negative values.
- abs(number) returns absolute value.

## Variables and control flow

let creates an immutable local; var creates a mutable local whose assigned values must keep the inferred/declared type. Blocks create lexical scopes. if/else and while require Boolean conditions. Loop-local let declarations remain rejected until safe loop-slot reset semantics are available; var can be used inside loops.

## Current limitations

- Aggregate types, arrays/structs in source, user-defined types, generics, interfaces, union types, closures and modules/imports are not yet implemented.
- The native C emitter does not yet lower user-defined functions; these execute through QVM v3.
- The C emitter reports unsupported source constructs rather than silently skipping them.
- AI and GPU provider interfaces do not include bundled inference engines or production vendor accelerator implementations.
- Identifiers remain ASCII pending Unicode normalization and anti-confusable handling.
