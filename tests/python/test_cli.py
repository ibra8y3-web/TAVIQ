from pathlib import Path
import subprocess
import sys
import tempfile

def run(args, cwd, expect=0):
    result = subprocess.run(args, cwd=cwd, text=True, capture_output=True, timeout=60)
    if result.returncode != expect:
        raise AssertionError("command %r returned %s\nstdout=%s\nstderr=%s" %
                             (args, result.returncode, result.stdout, result.stderr))
    return result

def main():
    binary = Path(sys.argv[1]).resolve()
    c_compiler = sys.argv[2] if len(sys.argv) > 2 else ""
    with tempfile.TemporaryDirectory(prefix="taviq-cli-") as directory:
        root = Path(directory)
        source = root / "smoke.taviq"
        source.write_text('flow main { let x = 6 * 7; display x; display "native-ready"; }\n', encoding="utf-8")
        assert "42" in run([str(binary), "run", str(source)], root).stdout
        assert "valid" in run([str(binary), "check", str(source)], root).stdout
        bytecode = root / "smoke.tvqbc"
        run([str(binary), "compile", str(source), "-o", str(bytecode)], root)
        assert "42" in run([str(binary), "run-bytecode", str(bytecode)], root).stdout
        assert "HALT" in run([str(binary), "disasm", str(bytecode)], root).stdout

        generated_c = root / "smoke.c"
        run([str(binary), "emit-c", str(source), "-o", str(generated_c)], root)
        native_source = root / "native-source"
        native_build = root / "native-build"
        native_source.mkdir()
        (native_source / "main.c").write_text(generated_c.read_text(encoding="utf-8"), encoding="utf-8")
        cmakelists = [
            "cmake_minimum_required(VERSION 3.20)",
            "project(TAVIQNativeSmoke LANGUAGES C)",
            "add_executable(taviq_native_smoke main.c)",
            ""
        ]
        (native_source / "CMakeLists.txt").write_text("\n".join(cmakelists), encoding="utf-8")
        configure = ["cmake", "-S", str(native_source), "-B", str(native_build), "-DCMAKE_BUILD_TYPE=Release"]
        if c_compiler:
            configure.append("-DCMAKE_C_COMPILER=" + c_compiler)
        run(configure, root)
        run(["cmake", "--build", str(native_build), "--config", "Release", "--parallel"], root)

        candidates = [
            native_build / "Release" / "taviq_native_smoke.exe",
            native_build / "taviq_native_smoke.exe",
            native_build / "Release" / "taviq_native_smoke",
            native_build / "taviq_native_smoke"
        ]
        executable = next((p for p in candidates if p.is_file()), None)
        if executable is None:
            raise AssertionError("native C executable not found in CMake build output")
        native = run([str(executable)], root)
        assert "42" in native.stdout and "native-ready" in native.stdout
    return 0

if __name__ == "__main__":
    raise SystemExit(main())
