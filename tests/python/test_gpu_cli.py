#!/usr/bin/env python3
"""Exercise the public GPU vector-add CLI through the ABI fixture provider."""
import subprocess
import sys

def main():
    if len(sys.argv) != 3:
        print("usage: test_gpu_cli.py <taviq> <provider>")
        return 2
    command = [sys.argv[1], "gpu-add", sys.argv[2], "0", "1,2,-3", "4,5,7"]
    result = subprocess.run(command, capture_output=True, text=True, check=False, timeout=15)
    if result.returncode != 0:
        print(result.stdout, end="")
        print(result.stderr, end="", file=sys.stderr)
        return 1
    if result.stdout.strip() != "5,7,4":
        print("unexpected gpu-add output: " + repr(result.stdout), file=sys.stderr)
        return 1
    bad = subprocess.run([sys.argv[1], "gpu-add", sys.argv[2], "0", "nan", "1"],
                         capture_output=True, text=True, check=False, timeout=15)
    if bad.returncode == 0:
        print("gpu-add unexpectedly accepted a non-finite value", file=sys.stderr)
        return 1
    return 0

if __name__ == "__main__":
    raise SystemExit(main())
