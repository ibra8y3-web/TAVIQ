# Platform Support Matrix

| Platform | CI status | Implemented scope |
|---|---|---|
| Linux x86_64 | build and tests | C++20 CLI, QVM, native C compiler smoke tests and Python tools |
| Windows x64 | build and tests | C++20 CLI, QVM, native C compiler smoke tests and Python tools |
| macOS | build and tests | C++20 CLI, QVM, native C compiler smoke tests and Python tools |
| Android arm64-v8a | NDK cross-build and Gradle APK build | JNI-backed Android application and native QVM runtime |
| Android emulator | instrumentation workflow | launches the Android test APK and runs native-runtime integration tests on an x86_64 emulator |

The Android application currently provides a small bilingual source editor, local project save/open and asynchronous compile/run through JNI. The Gradle configuration packages arm64-v8a and x86_64 ABIs. CI builds the debug APK and runs instrumentation tests on an emulator.

CI success is not physical-device validation. Testing on a real handset, broader lifecycle/storage permission coverage, UI automation, release signing and Play-distribution configuration remain outstanding.

GPU backends are not bundled. Accelerators must be tested on real devices and pass correctness/fallback tests before hardware support is declared.
