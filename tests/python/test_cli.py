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
    repo = Path(__file__).resolve().parents[2]
    dev = repo / "tools" / "taviq_dev.py"
    with tempfile.TemporaryDirectory(prefix="taviq-cli-") as directory:
        root = Path(directory)
        source = root / "smoke.taviq"
        source.write_text('flow main { let x = sqrt(36) + abs(-6); display x; display len("abc"); }\n', encoding="utf-8")
        assert "12" in run([str(binary), "run", str(source)], root).stdout
        assert "valid" in run([str(binary), "check", str(source)], root).stdout
        bytecode = root / "smoke.tvqbc"
        run([str(binary), "compile", str(source), "-o", str(bytecode)], root)
        assert "12" in run([str(binary), "run-bytecode", str(bytecode)], root).stdout
        assert "LENGTH" in run([str(binary), "disasm", str(bytecode)], root).stdout

        formatted = run([sys.executable, str(dev), "format", str(source)], root)
        assert "formatted:" in formatted.stdout
        run([sys.executable, str(dev), "format", "--check", str(source)], root)
        run([sys.executable, str(dev), "lint", str(source), "--compiler", str(binary)], root)
        assert "LENGTH" in run([sys.executable, str(dev), "inspect", str(source), "--compiler", str(binary)], root).stdout
        assert "profile runs=3" in run([sys.executable, str(dev), "profile", str(source), "--compiler", str(binary), "--runs", "3"], root).stdout

        typed = root / "typed.taviq"
        typed.write_text('flow main { let count:number=3; var label:string="ready"; var active:bool=true; display count; display label; display active; }\n', encoding="utf-8")
        assert run([str(binary), "run", str(typed)], root).stdout == "3\nready\ntrue\n"
        run([sys.executable, str(dev), "format", str(typed)], root)
        run([sys.executable, str(dev), "format", "--check", str(typed)], root)
        run([sys.executable, str(dev), "lint", str(typed), "--compiler", str(binary)], root)

        invalid_type = root / "invalid-type.taviq"
        invalid_type.write_text('flow main { let count:number="bad"; }\n', encoding="utf-8")
        run([str(binary), "check", str(invalid_type)], root, expect=1)

        functions = root / "functions.taviq"
        functions.write_text("""fn add(a: number, b: number) -> number {
    return a + b;
}
fn factorial(n: number) -> number {
    if (n <= 1) {
        return 1;
    } else {
        return n * factorial(n - 1);
    }
}
flow main {
    display add(20, 22);
    display factorial(6);
}
""", encoding="utf-8")
        assert run([str(binary), "run", str(functions)], root).stdout == "42\n720\n"
        function_bytecode = root / "functions.tvqbc"
        run([str(binary), "compile", str(functions), "-o", str(function_bytecode)], root)
        assert run([str(binary), "run-bytecode", str(function_bytecode)], root).stdout == "42\n720\n"
        assert "CALL" in run([str(binary), "disasm", str(function_bytecode)], root).stdout
        run([sys.executable, str(dev), "format", str(functions)], root)
        run([sys.executable, str(dev), "format", "--check", str(functions)], root)
        run([sys.executable, str(dev), "lint", str(functions), "--compiler", str(binary)], root)
        structured = root / "structured.taviq"
        structured.write_text(
            'flow main { var n=0; while(n<3){ n=n+1; if(n==2){display true;}else{display false;} } }\n',
            encoding="utf-8"
        )
        structured_result = run([str(binary), "run", str(structured)], root)
        assert structured_result.stdout == "false\ntrue\nfalse\n"
        traced = run([str(binary), "debug", str(structured)], root)
        assert "TRACE " in traced.stderr
        stopped = run([str(binary), "debug", str(structured), "0"], root)
        assert "BREAK 0" in stopped.stderr
        structured_bytecode = root / "structured.tvqbc"
        run([str(binary), "compile", str(structured), "-o", str(structured_bytecode)], root)
        assert run([str(binary), "run-bytecode", str(structured_bytecode)], root).stdout == structured_result.stdout
        run([sys.executable, str(dev), "format", str(structured)], root)
        run([sys.executable, str(dev), "format", "--check", str(structured)], root)
        run([sys.executable, str(dev), "lint", str(structured), "--compiler", str(binary)], root)

        generated_c = root / "smoke.c"
        run([str(binary), "emit-c", str(source), "-o", str(generated_c)], root)
        native_source = root / "native-source"
        native_build = root / "native-build"
        native_source.mkdir()
        (native_source / "main.c").write_text(generated_c.read_text(encoding="utf-8"), encoding="utf-8")
        cmakelists = ["cmake_minimum_required(VERSION 3.20)",
            "project(TAVIQNativeSmoke LANGUAGES C)", "add_executable(taviq_native_smoke main.c)",
            "if(NOT MSVC)", "  target_link_libraries(taviq_native_smoke PRIVATE m)", "endif()", ""]
        (native_source / "CMakeLists.txt").write_text("\n".join(cmakelists), encoding="utf-8")
        configure = ["cmake", "-S", str(native_source), "-B", str(native_build), "-DCMAKE_BUILD_TYPE=Release"]
        if c_compiler:
            configure.append("-DCMAKE_C_COMPILER=" + c_compiler)
        run(configure, root)
        run(["cmake", "--build", str(native_build), "--config", "Release", "--parallel"], root)
        candidates = [native_build / "Release" / "taviq_native_smoke.exe",
            native_build / "taviq_native_smoke.exe", native_build / "Release" / "taviq_native_smoke",
            native_build / "taviq_native_smoke"]
        executable = next((p for p in candidates if p.is_file()), None)
        if executable is None:
            raise AssertionError("native C executable not found")
        native = run([str(executable)], root)
        assert "12" in native.stdout and "3" in native.stdout

        structured_native = root / "structured-native.taviq"
        structured_native.write_text(
            """flow main {
    var i = 0;
    var total = 0;
    while (i < 4) {
        i = i + 1;
        if (i == 2) {
            total = total + 10;
        } else {
            total = total + i;
        }
    }
    display total;
    display true and not false;
}
""",
            encoding="utf-8"
        )
        structured_c = root / "structured-native.c"
        run([str(binary), "emit-c", str(structured_native), "-o", str(structured_c)], root)
        structured_source = root / "structured-native-source"
        structured_build = root / "structured-native-build"
        structured_source.mkdir()
        (structured_source / "main.c").write_text(
            structured_c.read_text(encoding="utf-8"), encoding="utf-8"
        )
        (structured_source / "CMakeLists.txt").write_text(
            """cmake_minimum_required(VERSION 3.20)
project(TAVIQStructuredNative LANGUAGES C)
add_executable(taviq_structured_native main.c)
if(NOT MSVC)
    target_link_libraries(taviq_structured_native PRIVATE m)
endif()
""",
            encoding="utf-8"
        )
        run(["cmake", "-S", str(structured_source), "-B", str(structured_build),
             "-DCMAKE_BUILD_TYPE=Release"], root)
        run(["cmake", "--build", str(structured_build), "--config", "Release", "--parallel"], root)
        native_candidates = [structured_build / "Release" / "taviq_structured_native.exe",
            structured_build / "taviq_structured_native.exe", structured_build / "Release" / "taviq_structured_native",
            structured_build / "taviq_structured_native"]
        native_executable = next((p for p in native_candidates if p.is_file()), None)
        if native_executable is None:
            raise AssertionError("structured native executable not found")
        structured_result = run([str(native_executable)], root)
        assert structured_result.stdout == "18\ntrue\n"
    return 0

if __name__ == "__main__":
    raise SystemExit(main())
