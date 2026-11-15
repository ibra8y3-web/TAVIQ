# TAVIQ Standard Library

The initial standard library is implemented as compiler/runtime intrinsics.

| Function | Signature | Behavior |
|---|---|---|
| len | len(string) -> number | UTF-8 byte length, not grapheme count |
| sqrt | sqrt(number) -> number | Square root; negative values fail at runtime |
| abs | abs(number) -> number | Absolute value |

All functions require exactly one argument. The type checker validates calls before bytecode is emitted. QVM bytecode has dedicated operations; the native C emitter lowers them to strlen, sqrt and fabs.

This is an initial core. Collections, filesystem, networking, dates, randomness, serialization and platform APIs remain to be implemented.
