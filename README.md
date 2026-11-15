# TAVIQ — AI-Native Computing Language

**Beyond Code. Beyond Limits.**

TAVIQ is an independent, open-source programming-language project focused on AI-native computing,
high-performance execution, safe runtimes, and cross-platform development.

> **Repository status:** Foundation / experimental prototype. This repository contains a real,
> buildable C++20 command-line prototype and the architecture, specifications, module boundaries,
> workflows, and implementation plans for the full language. It does not claim that all 16 systems
> are production-complete.

## Current working prototype

The first prototype recognizes and executes this small TAVIQ program:

```taviq
flow main {
    display "Hello from TAVIQ";
}
```

Build:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
./build/taviq examples/hello.taviq
```

On Windows, run `build/Release/taviq.exe examples/hello.taviq` when using a multi-config generator.

## Repository map

- `compiler/` — lexer, parser, syntax tree, semantic analysis, compiler driver.
- `ir/` — TAVIQ Intermediate Representation design.
- `runtime/` — initial execution engine and future VM contracts.
- `ai/` — provider-neutral AI engine interfaces and security requirements.
- `compute/` — parallel and accelerator abstractions.
- `memory/` — memory model and ownership design.
- `security/` — capability and sandbox specifications.
- `stdlib/` — standard-library module contracts.
- `package/` — package manager and registry design.
- `tools/` — formatter, linter, debugger and profiler plans.
- `studio/` — IDE architecture.
- `platforms/` — Windows, Linux, macOS and Android integration.
- `tests/` — executable prototype tests and future test suites.
- `docs/` — language and engineering specifications.
- `roadmap/` — 16 implementation stages.
- `logo/` — original procedural 3D logo source (Blender Python), plus vector artwork.

## Principles

1. No fake implementations or fake success states.
2. No claim of platform or model support without working implementation and verification.
3. All external dependencies must be pinned, reviewed, and license-compatible.
4. AI providers are adapters; TAVIQ does not depend on one vendor.
5. The language specification is versioned.
6. Security boundaries and permissions are explicit.
7. Every implemented feature must have tests and documentation.

## License

Apache-2.0. See `LICENSE`. Third-party components retain their own licenses.
