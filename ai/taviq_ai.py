#!/usr/bin/env python3
"""TAVIQ client for a locally running llama.cpp server (OpenAI-free, loopback by default)."""
import argparse
import json
import sys
import urllib.error
import urllib.parse
import urllib.request

def main():
    p = argparse.ArgumentParser(description="Send a prompt to a llama.cpp HTTP server")
    p.add_argument("--endpoint", default="http://127.0.0.1:8080")
    p.add_argument("--prompt", required=True)
    p.add_argument("--max-tokens", type=int, default=256)
    p.add_argument("--temperature", type=float, default=0.7)
    p.add_argument("--allow-remote", action="store_true", help="allow a non-loopback endpoint")
    args = p.parse_args()
    u = urllib.parse.urlparse(args.endpoint)
    if u.scheme != "http" and u.scheme != "https":
        p.error("endpoint must use http or https")
    if not args.allow_remote and u.hostname not in ("127.0.0.1", "::1", "localhost"):
        p.error("remote endpoint refused; use --allow-remote only on a trusted network")
    if not 1 <= args.max_tokens <= 8192:
        p.error("--max-tokens must be between 1 and 8192")
    payload = json.dumps({"prompt": args.prompt, "n_predict": args.max_tokens,
                          "temperature": args.temperature, "stream": False}).encode()
    req = urllib.request.Request(args.endpoint.rstrip("/") + "/completion", data=payload,
        headers={"Content-Type": "application/json", "Accept": "application/json"}, method="POST")
    try:
        with urllib.request.urlopen(req, timeout=180) as response:
            data = json.loads(response.read(8 * 1024 * 1024).decode("utf-8"))
        text = data.get("content")
        if not isinstance(text, str):
            raise ValueError("server response has no string 'content' field")
        sys.stdout.write(text)
    except (urllib.error.URLError, TimeoutError, ValueError, json.JSONDecodeError) as exc:
        print("TAVIQ AI:", exc, file=sys.stderr)
        return 1
    return 0

if __name__ == "__main__":
    raise SystemExit(main())
