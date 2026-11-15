#!/usr/bin/env python3
"""TAVIQ package manager: SHA-256 verified archives, safe extraction, no install-time scripts."""
import argparse
import hashlib
import json
import os
from pathlib import Path, PurePosixPath
import re
import shutil
import stat
import sys
import tarfile
import tempfile
import urllib.error
import urllib.parse
import urllib.request
import zipfile

MAX_ARCHIVE = 512 * 1024 * 1024
MAX_FILES = 10000
MAX_UNPACKED = 1024 * 1024 * 1024

def valid_name(value):
    if not re.fullmatch(r"[A-Za-z0-9][A-Za-z0-9_.-]{0,79}", value):
        raise ValueError("invalid package name")
    return value

def valid_version(value):
    if not isinstance(value, str) or not re.fullmatch(r"[A-Za-z0-9][A-Za-z0-9.+_-]{0,79}", value):
        raise ValueError("invalid package version")
    return value

def digest(path):
    h = hashlib.sha256()
    with path.open("rb") as f:
        for block in iter(lambda: f.read(1024 * 1024), b""):
            h.update(block)
    return h.hexdigest()

def download(url, target):
    u = urllib.parse.urlparse(url)
    if u.scheme != "https" or not u.hostname:
        raise ValueError("package URLs must use HTTPS")
    req = urllib.request.Request(url, headers={"User-Agent": "TAVIQ-Package-Manager/0.2"})
    with urllib.request.urlopen(req, timeout=90) as r, target.open("wb") as out:
        if urllib.parse.urlparse(r.geturl()).scheme != "https":
            raise ValueError("refused non-HTTPS package redirect")
        total = 0
        while True:
            block = r.read(1024 * 1024)
            if not block:
                break
            total += len(block)
            if total > MAX_ARCHIVE:
                raise ValueError("package archive exceeds 512 MiB")
            out.write(block)

def safe_target(root, member):
    if "\\" in member or re.match(r"^[A-Za-z]:", member):
        raise ValueError("unsafe archive path: " + member)
    p = PurePosixPath(member)
    if p.is_absolute() or not p.parts or any(part in ("", ".", "..") for part in p.parts):
        raise ValueError("unsafe archive path: " + member)
    target = root.joinpath(*p.parts)
    resolved = target.resolve()
    if root.resolve() not in resolved.parents and resolved != root.resolve():
        raise ValueError("archive path escapes package directory")
    return target

def extract_archive(archive, target):
    target.mkdir(parents=True, exist_ok=True)
    count = total = 0
    if zipfile.is_zipfile(archive):
        with zipfile.ZipFile(archive) as z:
            for item in z.infolist():
                if item.filename in ("", ".", "./"):
                    continue
                count += 1
                mode = (item.external_attr >> 16) & 0xFFFF
                if count > MAX_FILES or stat.S_ISLNK(mode):
                    raise ValueError("archive contains too many entries or a symbolic link")
                dest = safe_target(target, item.filename)
                if item.is_dir():
                    dest.mkdir(parents=True, exist_ok=True)
                    continue
                total += item.file_size
                if total > MAX_UNPACKED:
                    raise ValueError("unpacked package exceeds 1 GiB")
                dest.parent.mkdir(parents=True, exist_ok=True)
                with z.open(item) as src, dest.open("wb") as dst:
                    shutil.copyfileobj(src, dst)
    else:
        with tarfile.open(archive, "r:*") as t:
            for item in t:
                if item.name in ("", ".", "./"):
                    continue
                count += 1
                if count > MAX_FILES or not (item.isfile() or item.isdir()):
                    raise ValueError("tar archive contains links or unsupported special files")
                dest = safe_target(target, item.name)
                if item.isdir():
                    dest.mkdir(parents=True, exist_ok=True)
                    continue
                total += item.size
                if total > MAX_UNPACKED:
                    raise ValueError("unpacked package exceeds 1 GiB")
                dest.parent.mkdir(parents=True, exist_ok=True)
                source = t.extractfile(item)
                if source is None:
                    raise ValueError("cannot read archive member")
                with source, dest.open("wb") as dst:
                    shutil.copyfileobj(source, dst)

def load_index(index):
    if index.startswith("https://"):
        req = urllib.request.Request(index, headers={"User-Agent": "TAVIQ-Package-Manager/0.2"})
        with urllib.request.urlopen(req, timeout=30) as r:
            if urllib.parse.urlparse(r.geturl()).scheme != "https":
                raise ValueError("refused non-HTTPS registry redirect")
            return json.loads(r.read(8 * 1024 * 1024).decode("utf-8"))
    return json.loads(Path(index).expanduser().read_text(encoding="utf-8"))

def main():
    p = argparse.ArgumentParser(description="TAVIQ package manager")
    sub = p.add_subparsers(dest="command", required=True)
    ins = sub.add_parser("install")
    ins.add_argument("name"); ins.add_argument("--version", required=True)
    ins.add_argument("--index", required=True, help="HTTPS index URL or local JSON index")
    ins.add_argument("--root", default="."); ins.add_argument("--force", action="store_true")
    rem = sub.add_parser("remove"); rem.add_argument("name"); rem.add_argument("--version")
    rem.add_argument("--root", default=".")
    ls = sub.add_parser("list"); ls.add_argument("--root", default=".")
    args = p.parse_args(); root = Path(args.root).resolve()
    package_root = root / ".taviq" / "packages"; lock_path = root / "taviq.lock"
    try:
        lock = json.loads(lock_path.read_text(encoding="utf-8")) if lock_path.exists() else {"format": 1, "packages": {}}
        if args.command == "list":
            for name, versions in sorted(lock.get("packages", {}).items()):
                print(name, ", ".join(sorted(versions)))
            return 0
        name = valid_name(args.name)
        if args.command == "remove":
            versions = lock.get("packages", {}).get(name, {})
            targets = [args.version] if args.version else list(versions)
            for version in targets:
                version = valid_version(version)
                if version not in versions:
                    raise ValueError("package version is not installed: %s@%s" % (name, version))
                shutil.rmtree(package_root / name / version, ignore_errors=True)
                del versions[version]
            if not versions:
                lock.get("packages", {}).pop(name, None)
            tmp = lock_path.with_suffix(".lock.part"); tmp.write_text(json.dumps(lock, indent=2), encoding="utf-8"); os.replace(tmp, lock_path)
            return 0
        version = valid_version(args.version)
        index = load_index(args.index)
        entry = index.get("packages", {}).get(name, {}).get("versions", {}).get(version)
        if not isinstance(entry, dict):
            raise ValueError("package/version not found in registry index")
        url, expected = entry.get("url"), entry.get("sha256", "").lower()
        if not isinstance(url, str) or not re.fullmatch(r"[0-9a-f]{64}", expected):
            raise ValueError("registry entry must supply an HTTPS archive URL and SHA-256")
        dest = package_root / name / version
        if dest.exists() and not args.force:
            raise ValueError("package is already installed; pass --force to replace it")
        package_root.mkdir(parents=True, exist_ok=True)
        with tempfile.TemporaryDirectory(prefix="taviq-pkg-", dir=package_root) as temp:
            tmpdir = Path(temp); archive = tmpdir / "package.archive"
            download(url, archive)
            actual = digest(archive)
            if actual != expected:
                raise ValueError("archive SHA-256 mismatch")
            staging = tmpdir / "contents"; extract_archive(archive, staging)
            if dest.exists():
                shutil.rmtree(dest)
            dest.parent.mkdir(parents=True, exist_ok=True); os.replace(staging, dest)
        lock.setdefault("packages", {}).setdefault(name, {})[version] = {
            "sha256": expected, "source": url, "path": str(dest.relative_to(root))}
        tmp = lock_path.with_suffix(".lock.part"); tmp.write_text(json.dumps(lock, indent=2), encoding="utf-8"); os.replace(tmp, lock_path)
        print("installed", name + "@" + version, "without executing package scripts")
        return 0
    except (OSError, ValueError, KeyError, tarfile.TarError, zipfile.BadZipFile,
            urllib.error.URLError, json.JSONDecodeError) as exc:
        print("taviq-pkg:", exc, file=sys.stderr); return 1

if __name__ == "__main__":
    raise SystemExit(main())
