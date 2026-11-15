# Platform Support Matrix

| Platform | CI status | Scope |
|---|---|---|
| Linux x86_64 | build and tests | native C++20 CLI, QVM, Python tools |
| Windows x64 | build and tests | native C++20 CLI, QVM, Python tools |
| macOS | build and tests | native C++20 CLI, QVM, Python tools |
| Android arm64-v8a | NDK cross-build | CLI target compilation only; no Android app wrapper or device execution test |

CI success does not imply that every OS integration is implemented. Filesystem, threads and process launching use portable standard APIs. Android-specific app lifecycle, storage permissions, JNI/API integration and device testing are still required before Android can be advertised as a runtime-supported application platform.

GPU backends are not bundled. Accelerators must be tested on real devices and must pass correctness and fallback tests before support is declared.
