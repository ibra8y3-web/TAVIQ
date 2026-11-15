# TAVIQ Dependency Policy

TAVIQ currently aims to keep its runtime dependency surface small and auditable.

## Current approved build dependencies

- A C++20 compiler and its standard library.
- CMake 3.20 or newer.
- The platform thread library exposed through CMake Threads.
- Python 3 for repository tools and integration/security checks.

The Python tools use only the Python standard library. Model weights and optional inference servers are user-selected external assets; they are not vendored or executed as Python packages by the installer.

## Change control

A new third-party library or Python package must be proposed with its upstream source, version pin, license, maintenance status, and security implications. Update tools/check_dependencies.py, this document, CI and the relevant lock/manifest files in the same change.

The automated dependency policy check detects non-standard Python imports and unreviewed CMake dependency declarations. It does not replace vulnerability scanning of newly approved dependencies. GitHub's Dependency Review action can additionally be enabled after the repository Dependency Graph feature is turned on in repository security settings.
