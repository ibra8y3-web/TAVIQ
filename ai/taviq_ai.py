#!/usr/bin/env python3
"""TAVIQ client for a locally running llama.cpp server (loopback by default)."""
import argparse
import json
import math
import sys
import urllib.error
import urllib.parse
import urllib.request

def validate_endpoint(endpoint, allow_remote=False):
    parsed = urllib.parse.urlparse(endpoint)
    if parsed.scheme not in ("http", "https") or not parsed.hostname:
        raise ValueError("endpoint must be an absolute http or https URL")
    if parsed.username or parsed.password or parsed.query or parsed.fragment:
        raise ValueError("endpoint must not contain credentials, a query or a fragment")
    if not allow_remote and parsed.hostname not in ("127.0.0.1", "::1", "localhost"):
        raise ValueError("remote endpoint refused; explicit remote access is required")
    return endpoint.rstrip("/") + "/completion"

def validate_generation(prompt, max_tokens, temperature):
    if not isinstance(prompt, str) or not prompt:
        raise ValueError("prompt must be a non-empty string")
    if len(prompt.encode("utf-8")) > 1_000_000:
        raise ValueError("prompt exceeds the 1 MiB request limit")
    if not 1 <= max_tokens <= 8192:
        raise ValueError("--max-tokens must be between 1 and 8192")
    if not math.isfinite(temperature) or not 0.0 <= temperature <= 2.0:
        raise ValueError("--temperature must be finite and between 0 and 2")

def main():
    p = argparse.ArgumentParser(description="Send a prompt to a llama.cpp HTTP server")
    p.add_argument("--endpoint", default="http://127.0.0.1:8080")
    p.add_argument("--prompt", required=True)
    p.add_argument("--max-tokens", type=int, default=256)
    p.add_argument("--temperature", type=float, default=0.7)
    p.add_argument("--allow-remote", action="store_true", help="allow a non-loopback endpoint")
    args = p.parse_args()
    try:
        url = validate_endpoint(args.endpoint, args.allow_remote)
        validate_generation(args.prompt, args.max_tokens, args.temperature)
    except ValueError as exc:
        p.error(str(exc))
    payload = json.dumps({"prompt": args.prompt, "n_predict": args.max_tokens,
                          "temperature": args.temperature, "stream": False}).encode()
    req = urllib.request.Request(url, data=payload,
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
