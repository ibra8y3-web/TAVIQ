#!/usr/bin/env python3
"""Compile and execute native C output containing typed and recursive TAVIQ functions."""
from pathlib import Path
import subprocess
import sys
import tempfile

SOURCE = """fn add(a: number, b: number) -> number {
    return a + b;
}
fn factorial(n: number) -> number {
    if (n <= 1) {
        return 1;
    } else {
        return n * factorial(n - 1);
    }
}
fn isPositive(value: number) -> bool {
    if (value > 0) {
        return true;
    } else {
        return false;
    }
}
flow main {
    display add(20, 22);
    display factorial(6);
    display isPositive(-1);
}
"""

def run(command, cwd, expected=0):
    result = subprocess.run(command, cwd=cwd, capture_output=True, text=True,
                            check=False, timeout=120)
    if result.returncode != expected:
        raise AssertionError(
            "command returned %d, expected %d: %r\nstdout:\n%s\nstderr:\n%s" %
            (result.returncode, expected, command, result.stdout, result.stderr)
        )
    return result

def main():
    if len(sys.argv) != 3:
        print("usage: test_native_functions.py <taviq> <c-compiler>")
        return 2
    binary = Path(sys.argv[1]).resolve()
    c_compiler = sys.argv[2]
    with tempfile.TemporaryDirectory(prefix="taviq-native-functions-") as temporary:
        root = Path(temporary)
        source = root / "functions.taviq"
        generated = root / "functions.c"
        source.write_text(SOURCE, encoding="utf-8")
        run([str(binary), "emit-c", str(source), "-o", str(generated)], root)
        cmake_source = root / "native-source"
        cmake_build = root / "native-build"
        cmake_source.mkdir()
        (cmake_source / "main.c").write_text(generated.read_text(encoding="utf-8"), encoding="utf-8")
        cmake = [
            "cmake_minimum_required(VERSION 3.20)",
            "project(TAVIQNativeFunctions LANGUAGES C)",
            "add_executable(taviq_native_functions main.c)",
            "if(NOT MSVC)",
            "    target_link_libraries(taviq_native_functions PRIVATE m)",
            "endif()",
            "",
        ]
        (cmake_source / "CMakeLists.txt").write_text("\n".join(cmake), encoding="utf-8")
        configure = ["cmake", "-S", str(cmake_source), "-B", str(cmake_build),
                     "-DCMAKE_BUILD_TYPE=Release"]
        if c_compiler:
            configure.append("-DCMAKE_C_COMPILER=" + c_compiler)
        run(configure, root)
        run(["cmake", "--build", str(cmake_build), "--config", "Release", "--parallel"], root)
        candidates = [
            cmake_build / "Release" / "taviq_native_functions.exe",
            cmake_build / "taviq_native_functions.exe",
            cmake_build / "Release" / "taviq_native_functions",
            cmake_build / "taviq_native_functions",
        ]
        executable = next((path for path in candidates if path.is_file()), None)
        if executable is None:
            raise AssertionError("native function executable was not produced")
        result = run([str(executable)], root)
        if result.stdout != "42\n720\nfalse\n":
            raise AssertionError("unexpected native function output: " + repr(result.stdout))

        invalid = root / "invalid-return.taviq"
        invalid.write_text(
            "fn incomplete(x: number) -> number { if (x > 0) { return x; } } "
            "flow main { display incomplete(1); }\n", encoding="utf-8"
        )
        rejected = run([str(binary), "emit-c", str(invalid), "-o", str(root / "invalid.c")],
                       root, expected=1)
        if "does not return on every path" not in rejected.stderr:
            raise AssertionError("native backend did not diagnose a missing return path")
    return 0

if __name__ == "__main__":
    raise SystemExit(main())
