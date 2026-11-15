#!/usr/bin/env python3
"""Fail-closed Linux process sandbox launcher using bubblewrap."""
import argparse
import os
from pathlib import Path
import shutil
import subprocess
import sys

def build_command(program, args, workspace, network=False, readonly=False):
    bwrap=shutil.which("bwrap")
    if not bwrap:
        raise RuntimeError("bubblewrap is required; refusing to run without OS isolation")
    workspace=Path(workspace).resolve()
    if not workspace.is_dir():
        raise ValueError("workspace must be an existing directory")
    command=[bwrap,"--die-with-parent","--new-session","--unshare-all",
             "--proc","/proc","--dev","/dev","--tmpfs","/tmp",
             "--ro-bind","/usr","/usr"]
    for directory in ("/bin","/sbin","/lib","/lib64"):
        if Path(directory).exists():
            command += ["--ro-bind",directory,directory]
    for file in ("/etc/ld.so.cache","/etc/alternatives"):
        if Path(file).exists():
            command += ["--ro-bind",file,file]
    if not network:
        command.append("--unshare-net")
    if readonly:
        command += ["--ro-bind",str(workspace),"/workspace"]
    else:
        command += ["--bind",str(workspace),"/workspace"]
    command += ["--chdir","/workspace","--",str(Path(program).resolve()),*args]
    return command

def main():
    parser=argparse.ArgumentParser(description="Run a trusted executable in a fail-closed Linux namespace sandbox")
    parser.add_argument("--workspace",required=True)
    parser.add_argument("--allow-network",action="store_true")
    parser.add_argument("--read-only",action="store_true")
    parser.add_argument("program")
    parser.add_argument("args",nargs=argparse.REMAINDER)
    args=parser.parse_args()
    try:
        command=build_command(args.program,args.args,args.workspace,args.allow_network,args.read_only)
        return subprocess.run(command,check=False,timeout=3600).returncode
    except (OSError,ValueError,RuntimeError,subprocess.TimeoutExpired) as exc:
        print("taviq-sandbox:",exc,file=sys.stderr)
        return 1
if __name__=="__main__":
    raise SystemExit(main())
