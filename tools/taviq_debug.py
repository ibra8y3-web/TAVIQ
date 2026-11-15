#!/usr/bin/env python3
"""Launch the TAVIQ QVM instruction tracer."""
import argparse
from pathlib import Path
import subprocess
import sys

def main():
    parser=argparse.ArgumentParser(description="Trace QVM instructions and select instruction-index breakpoints")
    parser.add_argument("source")
    parser.add_argument("--compiler",default="build/taviq")
    parser.add_argument("--break",dest="breakpoints",default="")
    args=parser.parse_args()
    source=Path(args.source).expanduser().resolve()
    compiler=Path(args.compiler).expanduser().resolve()
    if source.suffix!=".taviq" or not source.is_file():
        parser.error("source must be an existing .taviq file")
    if not compiler.is_file():
        parser.error("compiler executable does not exist")
    command=[str(compiler),"debug",str(source)]
    if args.breakpoints:
        command.append(args.breakpoints)
    try:
        return subprocess.run(command,check=False,timeout=180).returncode
    except (OSError,subprocess.TimeoutExpired) as exc:
        print("taviq-debug:",exc,file=sys.stderr)
        return 1
if __name__=="__main__":
    raise SystemExit(main())
