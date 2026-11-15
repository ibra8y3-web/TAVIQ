#!/usr/bin/env python3
"""Audit TAVIQ's approved runtime and optional build-tool dependencies."""
import ast
from pathlib import Path
import re
import sys

ALLOWED_CMAKE_PACKAGES = {"Threads", "Python3"}
OPTIONAL_IMPORTS = {
    # These modules are supplied by Blender's embedded Python when this
    # optional procedural-logo script is run inside Blender.
    "logo/taviq_3d_logo.py": {"bpy", "mathutils"},
}
SKIP_DIRS = {".git", "build", "build-android", "build-codeql", "__pycache__"}

def python_import_errors(root):
    errors = []
    stdlib = getattr(sys, "stdlib_module_names", set())
    for path in sorted(root.rglob("*.py")):
        relative = path.relative_to(root).as_posix()
        if any(part in SKIP_DIRS for part in path.parts):
            continue
        try:
            tree = ast.parse(path.read_text(encoding="utf-8"), filename=str(path))
        except (OSError, SyntaxError) as exc:
            errors.append("%s: cannot parse Python source: %s" % (relative, exc))
            continue
        permitted_optional = OPTIONAL_IMPORTS.get(relative, set())
        for node in ast.walk(tree):
            names = []
            if isinstance(node, ast.Import):
                names = [item.name.split(".", 1)[0] for item in node.names]
            elif isinstance(node, ast.ImportFrom) and node.level == 0 and node.module:
                names = [node.module.split(".", 1)[0]]
            for name in names:
                if name not in stdlib and name not in {"__future__"} and name not in permitted_optional:
                    errors.append("%s:%d: non-standard Python import '%s'" %
                                  (relative, node.lineno, name))
    return errors

def cmake_dependency_errors(root):
    path = root / "CMakeLists.txt"
    try:
        source = path.read_text(encoding="utf-8")
    except OSError as exc:
        return ["CMakeLists.txt: %s" % exc]
    errors = []
    for package in re.findall(r"(?im)^\s*find_package\s*\(\s*([A-Za-z0-9_]+)", source):
        if package not in ALLOWED_CMAKE_PACKAGES:
            errors.append("CMakeLists.txt: unreviewed find_package('%s')" % package)
    for command in ("FetchContent_Declare", "ExternalProject_Add", "CPMAddPackage"):
        if re.search(r"\b" + command + r"\b", source):
            errors.append("CMakeLists.txt: unreviewed external dependency mechanism '%s'" % command)
    return errors

def audit(root):
    root = Path(root).resolve()
    return python_import_errors(root) + cmake_dependency_errors(root)

def main():
    root = Path(__file__).resolve().parents[1]
    errors = audit(root)
    if errors:
        for error in errors:
            print("dependency policy:", error, file=sys.stderr)
        return 1
    print("Dependency policy passed: Python standard library; Blender API only in its optional logo script; CMake Threads/Python3 only.")
    return 0

if __name__ == "__main__":
    raise SystemExit(main())
