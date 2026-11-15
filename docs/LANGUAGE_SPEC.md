# TAVIQ Language Specification — Draft 0.1

## 1. Identity
TAVIQ is an independent AI-native computing language. Its source extension is `.taviq`;
module extension is `.tavim`; project manifest is `taviq.toml`.

## 2. Paradigms
The intended language supports imperative, functional, data-oriented, concurrent,
systems, and AI-oriented programming.

## 3. Lexical rules
UTF-8 source files. Identifiers are ASCII in prototype 0.1; Unicode identifiers are
reserved for a separately specified normalization and security policy. `//` begins
a line comment. Strings use double quotes and backslash escapes.

## 4. Prototype grammar
```ebnf
program      = "flow" "main" "{" { statement } "}" ;
statement    = "display" string_literal ";" ;
string_literal = '"' { character | escape } '"' ;
```

## 5. Planned grammar
The complete language will add declarations, functions, modules, types, generics,
pattern matching, tasks, async/await, parallel blocks, AI model declarations,
capability scopes, and error handling. These are design targets, not implemented
features in prototype 0.1.

## 6. Type system target
Static typing with inference, algebraic data types, generics, traits/interfaces,
explicit mutability, and safe ownership/reference rules.

## 7. Compatibility
Grammar and semantics changes are tracked by language edition. Breaking changes
require a major language edition and migration notes.
