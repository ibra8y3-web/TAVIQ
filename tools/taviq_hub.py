#!/usr/bin/env python3
"""Secure-by-default Hugging Face model asset installer for TAVIQ."""
import argparse
import hashlib
import json
import os
from pathlib import Path, PurePosixPath
import re
import sys
import urllib.error
import urllib.parse
import urllib.request

BASE = "https://huggingface.co"
SAFE_SUFFIXES = {".gguf", ".safetensors", ".json", ".txt", ".model", ".tiktoken"}
SAFE_NAMES = {"config.json", "generation_config.json", "tokenizer.json",
              "tokenizer_config.json", "special_tokens_map.json", "vocab.json",
              "merges.txt", "added_tokens.json", "tokenizer.model",
              "model.safetensors.index.json"}
MAX_FILE = 20 * 1024 * 1024 * 1024
MAX_TOTAL = 20 * 1024 * 1024 * 1024
MAX_SELECTED_FILES = 1000

def request(url, limit=32 * 1024 * 1024):
    req = urllib.request.Request(url, headers={"User-Agent": "TAVIQ-AI-Hub/0.2"})
    with urllib.request.urlopen(req, timeout=60) as response:
        if urllib.parse.urlparse(response.geturl()).scheme != "https":
            raise ValueError("refused non-HTTPS redirect")
        data = response.read(limit + 1)
    if len(data) > limit:
        raise ValueError("response exceeds configured safety limit")
    return data

def download_file(url, target, limit):
    req = urllib.request.Request(url, headers={"User-Agent": "TAVIQ-AI-Hub/0.2"})
    h = hashlib.sha256(); total = 0
    with urllib.request.urlopen(req, timeout=120) as response, target.open("wb") as out:
        if urllib.parse.urlparse(response.geturl()).scheme != "https":
            raise ValueError("refused non-HTTPS model redirect")
        while True:
            block = response.read(1024 * 1024)
            if not block:
                break
            total += len(block)
            if total > limit:
                raise ValueError("model file exceeds configured size limit")
            h.update(block); out.write(block)
    return total, h.hexdigest()

def safe_model_id(value):
    parts = value.split("/")
    if len(parts) != 2 or any(part in ("", ".", "..") for part in parts):
        raise ValueError("model id must be owner/name without traversal components")
    if any(not re.fullmatch(r"[A-Za-z0-9_.-]+", part) for part in parts):
        raise ValueError("model id contains unsupported characters")
    return value

def safe_file(value):
    if "\\" in value or ":" in value:
        raise ValueError("unsafe model filename: " + value)
    p = PurePosixPath(value)
    if p.is_absolute() or not p.parts or any(x in ("", ".", "..") for x in p.parts):
        raise ValueError("unsafe model filename: " + value)
    if p.suffix.lower() not in SAFE_SUFFIXES and p.name not in SAFE_NAMES:
        raise ValueError("file type is not allowed: " + value)
    if any(part.startswith(".") for part in p.parts):
        raise ValueError("hidden model paths are not allowed")
    return p

def validate_max_total(value):
    if not isinstance(value, int) or not 1 <= value <= MAX_TOTAL:
        raise ValueError("--max-total must be between 1 byte and 20 GiB")
    return value

def model_info(model_id, revision):
    endpoint = BASE + "/api/models/" + urllib.parse.quote(model_id, safe="/") + "?blobs=true"
    if revision:
        endpoint = BASE + "/api/models/" + urllib.parse.quote(model_id, safe="/") + "/revision/" + urllib.parse.quote(revision, safe="") + "?blobs=true"
    data = json.loads(request(endpoint).decode("utf-8"))
    sha = data.get("sha")
    if not isinstance(sha, str) or not re.fullmatch(r"[0-9a-f]{40}", sha):
        raise ValueError("Hub did not return an immutable 40-character commit SHA")
    return data, sha

def install(args):
    model_id = safe_model_id(args.model)
    max_total = validate_max_total(args.max_total)
    info, sha = model_info(model_id, args.revision)
    tags = info.get("tags") or []
    license_tags = [x.split(":", 1)[1] for x in tags if isinstance(x, str) and x.startswith("license:")]
    license_name = (info.get("cardData") or {}).get("license") or (license_tags[0] if license_tags else "unknown")
    if not args.accept_license:
        raise ValueError("license detected: %s; rerun with --accept-license after reviewing the model license" % license_name)
    siblings = info.get("siblings") or []
    available = {x.get("rfilename"): x for x in siblings if isinstance(x, dict) and isinstance(x.get("rfilename"), str)}
    selected = args.file or []
    if not selected:
        selected = [n for n in available if PurePosixPath(n).suffix.lower() == ".gguf"]
        if not selected:
            selected = [n for n in available if n in SAFE_NAMES]
    selected = list(dict.fromkeys(selected))
    if not selected:
        raise ValueError("no safe model assets selected; specify one or more --file values")
    if len(selected) > MAX_SELECTED_FILES:
        raise ValueError("too many selected model files")
    root = Path(args.destination).expanduser().resolve() / model_id.replace("/", "--")
    root.mkdir(parents=True, exist_ok=True)
    total = 0; records = []
    for name in selected:
        safe_file(name)
        if name not in available:
            raise ValueError("file is not present at the pinned model revision: " + name)
        meta = available[name]
        expected_size = int(meta.get("size") or (meta.get("lfs") or {}).get("size") or 0)
        if expected_size > MAX_FILE:
            raise ValueError("file is larger than the 20 GiB safety limit: " + name)
        remaining = min(MAX_FILE, max_total - total)
        if remaining <= 0:
            raise ValueError("selected files exceed --max-total")
        url = BASE + "/" + urllib.parse.quote(model_id, safe="/") + "/resolve/" + sha + "/" + urllib.parse.quote(name, safe="/")
        destination = root.joinpath(*safe_file(name).parts)
        destination.parent.mkdir(parents=True, exist_ok=True)
        temp = destination.with_name(destination.name + ".taviq-part")
        try:
            size, digest = download_file(url, temp, remaining)
            if expected_size and size != expected_size:
                raise ValueError("download size does not match Hub metadata for " + name)
            lfs = (meta.get("lfs") or {}).get("oid")
            if isinstance(lfs, str) and re.fullmatch(r"[0-9a-f]{64}", lfs) and digest != lfs:
                raise ValueError("SHA-256 mismatch for " + name)
            os.replace(temp, destination)
        finally:
            if temp.exists():
                temp.unlink()
        total += size
        records.append({"file": name, "sha256": digest, "bytes": size})
        print("installed", name, "%d bytes" % size)
    manifest = {"format": 1, "model": model_id, "revision": sha, "license": license_name,
                "license_accepted_by_user": True, "files": records}
    temp = root / ".taviq-model.json.part"
    temp.write_text(json.dumps(manifest, indent=2), encoding="utf-8")
    os.replace(temp, root / "taviq-model.json")
    print("model assets installed at", root)
    print("Inference requires a separately installed and configured provider such as llama.cpp.")

def main():
    p = argparse.ArgumentParser(description="Install pinned, allow-listed model assets from Hugging Face")
    p.add_argument("model", help="Hub model id, owner/name")
    p.add_argument("--revision", help="branch, tag or commit; resolved and pinned to a commit SHA")
    p.add_argument("--file", action="append", help="specific safe file to install (repeatable)")
    p.add_argument("--accept-license", action="store_true", help="confirm that you reviewed and accept the model license")
    p.add_argument("--destination", default="~/.taviq/models")
    p.add_argument("--max-total", type=int, default=8 * 1024 * 1024 * 1024, help="maximum total bytes")
    args = p.parse_args()
    try:
        install(args)
    except (ValueError, OSError, urllib.error.URLError, json.JSONDecodeError) as exc:
        print("TAVIQ Hub:", exc, file=sys.stderr)
        return 1
    return 0

if __name__ == "__main__":
    raise SystemExit(main())
