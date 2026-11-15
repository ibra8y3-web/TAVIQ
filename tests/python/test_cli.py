import os
from pathlib import Path
import subprocess
import sys
import tempfile

def run(args, cwd, expect=0):
    result = subprocess.run(args, cwd=cwd, text=True, capture_output=True, timeout=30)
    if result.returncode != expect:
        raise AssertionError("command %r returned %s\nstdout=%s\nstderr=%s" %
                             (args, result.returncode, result.stdout, result.stderr))
    return result

def main():
    binary = Path(sys.argv[1]).resolve()
    cc = sys.argv[2] if len(sys.argv) > 2 and sys.argv[2] else os.environ.get("CC", "cc")
    cc_id = sys.argv[3] if len(sys.argv) > 3 else ""
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
        cfile = root / "smoke.c"
        run([str(binary), "emit-c", str(source), "-o", str(cfile)], root)
        executable = root / ("smoke-native.exe" if os.name == "nt" else "smoke-native")
        if os.name == "nt" and cc_id.upper() == "MSVC":
            command = [cc, "/nologo", str(cfile), "/Fe:" + str(executable)]
        else:
            command = [cc, str(cfile), "-o", str(executable)]
        run(command, root)
        native = run([str(executable)], root)
        assert "42" in native.stdout and "native-ready" in native.stdout
    return 0

if __name__ == "__main__":
    raise SystemExit(main())
