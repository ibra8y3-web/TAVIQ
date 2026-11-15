#!/usr/bin/env python3
"""TAVIQ source formatter, compiler-backed linter, bytecode inspector and profiler."""
import argparse
from pathlib import Path
import re
import statistics
import subprocess
import sys
import tempfile
import time

TOKEN = re.compile(
    r'"(?:\\.|[^"\\])*"|==|!=|<=|>=|'
    r'[A-Za-z_][A-Za-z0-9_]*|(?:\d+(?:\.\d+)?(?:[eE][+-]?\d+)?)|[{}();,+*/=!<>-]'
)
BINARY_OPERATORS = {"=", "==", "!=", "<", "<=", ">", ">=", "+", "-", "*", "/", "and", "or"}

def tokenize(source):
    tokens = []
    i = 0
    while i < len(source):
        if source[i].isspace():
            i += 1
            continue
        if source.startswith("//", i):
            end = source.find("\n", i)
            i = len(source) if end < 0 else end + 1
            continue
        match = TOKEN.match(source, i)
        if not match:
            raise ValueError("formatter: unsupported character at offset %d" % i)
        tokens.append(match.group(0))
        i = match.end()
    return tokens

def format_source(source):
    tokens = tokenize(source)
    if len(tokens) < 4 or tokens[:3] != ["flow", "main", "{"] or tokens[-1] != "}":
        raise ValueError("formatter: expected one flow main block")

    lines = ["flow main {"]
    indent = 1
    current = ""
    previous = None
    paren_depth = 0

    def emit(text):
        lines.append(("    " * indent) + text.strip())

    for index, token in enumerate(tokens[3:-1], start=3):
        next_token = tokens[index + 1] if index + 1 < len(tokens) else None

        if token == "(":
            if previous in {"if", "while", "display", "let", "var"}:
                current = current.rstrip() + " "
            current += "("
            paren_depth += 1
        elif token == ")":
            paren_depth -= 1
            if paren_depth < 0:
                raise ValueError("formatter: unmatched ')'")
            current = current.rstrip() + ")"
        elif token == "{":
            if not current.strip():
                raise ValueError("formatter: unexpected '{'")
            emit(current.rstrip() + " {")
            indent += 1
            current = ""
        elif token == "}":
            if current.strip():
                emit(current)
                current = ""
            indent -= 1
            if indent < 1:
                raise ValueError("formatter: unmatched '}'")
            current = "}"
            if next_token != "else":
                emit(current)
                current = ""
        elif token == "else":
            if current == "}":
                current += " else"
            elif not current:
                current = "else"
            else:
                raise ValueError("formatter: unexpected else")
        elif token == ";":
            if not current.strip():
                raise ValueError("formatter: empty statement")
            emit(current.rstrip() + ";")
            current = ""
        elif token == ",":
            current = current.rstrip() + ", "
        elif token in BINARY_OPERATORS:
            current = current.rstrip() + " " + token + " "
        elif token == "!":
            current = current.rstrip() + "!"
        else:
            if current and previous not in {"(", ",", "!", *BINARY_OPERATORS}:
                current += " "
            current += token

        previous = token

    if paren_depth != 0:
        raise ValueError("formatter: unbalanced parentheses")
    if current.strip():
        if current == "}":
            emit(current)
        else:
            raise ValueError("formatter: statement is missing ';'")
    if indent != 1:
        raise ValueError("formatter: unbalanced braces")
    lines.append("}")
    return "\n".join(lines) + "\n"

def run_compiler(compiler, command, source, timeout=60):
    return subprocess.run([str(compiler), command, str(source)], text=True,
                          capture_output=True, timeout=timeout, check=False)

def main():
    parser = argparse.ArgumentParser(description="TAVIQ source formatter, linter, inspector and profiler")
    sub = parser.add_subparsers(dest="command", required=True)

    fmt = sub.add_parser("format", help="format a .taviq source file")
    fmt.add_argument("source")
    fmt.add_argument("--check", action="store_true", help="fail when formatting changes are required")

    lint = sub.add_parser("lint", help="run the compiler parser/type checker")
    lint.add_argument("source")
    lint.add_argument("--compiler", default="build/taviq")

    inspect = sub.add_parser("inspect", help="compile source and print verified QVM disassembly")
    inspect.add_argument("source")
    inspect.add_argument("--compiler", default="build/taviq")

    profile = sub.add_parser("profile", help="measure repeated source execution")
    profile.add_argument("source")
    profile.add_argument("--compiler", default="build/taviq")
    profile.add_argument("--runs", type=int, default=10)

    args = parser.parse_args()
    source = Path(args.source).expanduser().resolve()
    if not source.is_file() or source.suffix != ".taviq":
        parser.error("source must be an existing .taviq file")

    try:
        if args.command == "format":
            original = source.read_text(encoding="utf-8")
            formatted = format_source(original)
            if args.check:
                if original != formatted:
                    print("format required:", source, file=sys.stderr)
                    return 1
                print("format ok:", source)
                return 0
            temporary = source.with_name(source.name + ".taviq-format-part")
            temporary.write_text(formatted, encoding="utf-8")
            temporary.replace(source)
            print("formatted:", source)
            return 0

        compiler = Path(args.compiler).expanduser().resolve()
        if not compiler.is_file():
            raise ValueError("compiler executable not found: " + str(compiler))

        if args.command == "lint":
            result = run_compiler(compiler, "check", source)
            sys.stdout.write(result.stdout)
            sys.stderr.write(result.stderr)
            if result.returncode == 0:
                print("lint passed:", source)
            return result.returncode

        if args.command == "inspect":
            with tempfile.TemporaryDirectory(prefix="taviq-inspect-") as directory:
                bytecode = Path(directory) / "program.tvqbc"
                compiled = subprocess.run(
                    [str(compiler), "compile", str(source), "-o", str(bytecode)],
                    text=True, capture_output=True, timeout=60, check=False
                )
                if compiled.returncode != 0:
                    sys.stderr.write(compiled.stderr)
                    return compiled.returncode
                result = subprocess.run(
                    [str(compiler), "disasm", str(bytecode)],
                    text=True, capture_output=True, timeout=60, check=False
                )
                sys.stdout.write(result.stdout)
                sys.stderr.write(result.stderr)
                return result.returncode

        if args.runs < 1 or args.runs > 1000:
            raise ValueError("--runs must be between 1 and 1000")
        durations = []
        expected_output = None
        for _ in range(args.runs):
            start = time.perf_counter()
            result = run_compiler(compiler, "run", source, timeout=180)
            durations.append(time.perf_counter() - start)
            if result.returncode != 0:
                sys.stderr.write(result.stderr)
                return result.returncode
            if expected_output is None:
                expected_output = result.stdout
            elif result.stdout != expected_output:
                raise ValueError("program output changed between profiling runs")
        if expected_output:
            sys.stdout.write(expected_output)
        ordered = sorted(durations)
        p95 = ordered[min(len(ordered) - 1, int(0.95 * len(ordered)))]
        print("profile runs=%d mean_ms=%.3f median_ms=%.3f p95_ms=%.3f min_ms=%.3f" %
              (args.runs, statistics.mean(durations) * 1000,
               statistics.median(durations) * 1000, p95 * 1000, ordered[0] * 1000))
        return 0
    except (OSError, ValueError, subprocess.TimeoutExpired) as exc:
        print("taviq-dev:", exc, file=sys.stderr)
        return 1

if __name__ == "__main__":
    raise SystemExit(main())
