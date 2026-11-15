# Platform Support Policy

Targets: Windows, Linux, macOS, Android.

Initial development host recommendation: Linux x86_64. CI should add Windows and
macOS builds after the compiler has stable host-independent behavior. Android
requires NDK toolchains, ABI packaging, and Android-specific filesystem/thread/
dynamic-library policies. A target is supported only after CI builds and tests it.
