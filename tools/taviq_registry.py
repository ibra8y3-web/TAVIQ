#!/usr/bin/env python3
"""Create signed TAVIQ registry indexes and serve a read-only local registry."""
import argparse
from http.server import SimpleHTTPRequestHandler, ThreadingHTTPServer
import hashlib
import json
from pathlib import Path
import re
import subprocess
import sys
import tempfile
import urllib.parse

SEMVER = re.compile(r"^(0|[1-9]\d*)\.(0|[1-9]\d*)\.(0|[1-9]\d*)(?:-[0-9A-Za-z.-]+)?(?:\+[0-9A-Za-z.-]+)?$")
NAME = re.compile(r"^[A-Za-z0-9][A-Za-z0-9_.-]{0,79}$")

def atomic_write(path, data):
    path = Path(path)
    path.parent.mkdir(parents=True, exist_ok=True)
    temporary = path.with_name(path.name + ".part")
    temporary.write_bytes(data)
    temporary.replace(path)

def publish(args):
    name = args.name
    version = args.version
    archive = Path(args.archive).expanduser().resolve()
    index_path = Path(args.index).expanduser().resolve()
    key_path = Path(args.private_key).expanduser().resolve()
    parsed = urllib.parse.urlparse(args.base_url)
    if not NAME.fullmatch(name):
        raise ValueError("invalid package name")
    if not SEMVER.fullmatch(version):
        raise ValueError("version must be semantic major.minor.patch")
    if not archive.is_file() or not key_path.is_file():
        raise ValueError("package archive and Ed25519 private key must exist")
    if parsed.scheme != "https" or not parsed.hostname or parsed.username or parsed.password:
        raise ValueError("registry package base URL must be HTTPS without credentials")
    dependencies = {}
    if args.manifest:
        manifest = json.loads(Path(args.manifest).read_text(encoding="utf-8"))
        dependencies = manifest.get("dependencies", {})
        if not isinstance(dependencies, dict) or any(
            not isinstance(k, str) or not NAME.fullmatch(k) or not isinstance(v, str)
            for k, v in dependencies.items()
        ):
            raise ValueError("manifest dependencies must map package names to version ranges")
    if index_path.exists():
        index = json.loads(index_path.read_text(encoding="utf-8"))
    else:
        index = {"format": 1, "packages": {}}
    if not isinstance(index, dict) or not isinstance(index.get("packages"), dict):
        raise ValueError("registry index must contain a packages object")
    packages = index["packages"]
    package = packages.setdefault(name, {"versions": {}})
    versions = package.setdefault("versions", {})
    if version in versions:
        raise ValueError("package version is already published and immutable")
    digest = hashlib.sha256(archive.read_bytes()).hexdigest()
    url = args.base_url.rstrip("/") + "/" + archive.name
    versions[version] = {"url": url, "sha256": digest, "dependencies": dict(sorted(dependencies.items()))}
    encoded = (json.dumps(index, sort_keys=True, separators=(",", ":"), ensure_ascii=False) + "\n").encode("utf-8")
    signature_path = Path(args.signature).expanduser().resolve() if args.signature else Path(str(index_path)+".sig")
    with tempfile.TemporaryDirectory(prefix="taviq-registry-sign-") as directory:
        source = Path(directory) / "index.json"
        signature = Path(directory) / "index.sig"
        source.write_bytes(encoded)
        result = subprocess.run(
            ["openssl", "pkeyutl", "-sign", "-rawin", "-inkey", str(key_path),
             "-in", str(source), "-out", str(signature)],
            capture_output=True, text=True, timeout=30, check=False
        )
        if result.returncode != 0:
            raise ValueError("OpenSSL Ed25519 signing failed: " + result.stderr[:1024])
        signature_bytes = signature.read_bytes()
    atomic_write(index_path, encoded)
    atomic_write(signature_path, signature_bytes)
    print("published", name+"@"+version)
    print("sha256", digest)
    print("index", index_path)
    print("signature", signature_path)
    return 0

def serve(args):
    root = Path(args.root).expanduser().resolve()
    if not root.is_dir():
        raise ValueError("registry root directory does not exist")
    if args.host not in ("127.0.0.1", "::1", "localhost") and not args.allow_remote:
        raise ValueError("non-loopback serving requires --allow-remote and a TLS reverse proxy")
    handler = lambda *a, **kw: SimpleHTTPRequestHandler(*a, directory=str(root), **kw)
    server = ThreadingHTTPServer((args.host, args.port), handler)
    print("read-only TAVIQ registry at http://%s:%d/ (place behind HTTPS for remote clients)" %
          (args.host, args.port))
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        pass
    finally:
        server.server_close()
    return 0

def main():
    parser = argparse.ArgumentParser(description="TAVIQ signed package registry tools")
    sub = parser.add_subparsers(dest="command", required=True)
    pub = sub.add_parser("publish", help="publish a package into a signed JSON index")
    pub.add_argument("--name", required=True)
    pub.add_argument("--version", required=True)
    pub.add_argument("--archive", required=True)
    pub.add_argument("--base-url", required=True)
    pub.add_argument("--index", required=True)
    pub.add_argument("--private-key", required=True)
    pub.add_argument("--signature")
    pub.add_argument("--manifest")
    run = sub.add_parser("serve", help="serve an existing read-only registry directory")
    run.add_argument("--root", default="./registry")
    run.add_argument("--host", default="127.0.0.1")
    run.add_argument("--port", type=int, default=8766)
    run.add_argument("--allow-remote", action="store_true")
    args = parser.parse_args()
    try:
        if args.command == "publish":
            return publish(args)
        return serve(args)
    except (OSError, ValueError, KeyError, json.JSONDecodeError, subprocess.TimeoutExpired) as exc:
        print("taviq-registry:", exc, file=sys.stderr)
        return 1

if __name__ == "__main__":
    raise SystemExit(main())
