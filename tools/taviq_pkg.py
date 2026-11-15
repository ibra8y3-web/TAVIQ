#!/usr/bin/env python3
"""TAVIQ package manager: SHA-256 verified archives, safe extraction, no install-time scripts."""
import argparse
import hashlib
import json
import os
from pathlib import Path, PurePosixPath
import re
import shutil
import subprocess
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

SEMVER_RE = re.compile(r"^(0|[1-9]\d*)\.(0|[1-9]\d*)\.(0|[1-9]\d*)(?:-([0-9A-Za-z.-]+))?(?:\+[0-9A-Za-z.-]+)?$")

def version_key(value):
    match = SEMVER_RE.fullmatch(value)
    if not match:
        raise ValueError("invalid semantic version in registry: " + str(value))
    major, minor, patch = (int(match.group(i)) for i in (1, 2, 3))
    prerelease = match.group(4)
    if prerelease is None:
        pre_key = ((2, ""),)
    else:
        pre_key = tuple((0, int(part)) if part.isdigit() else (1, part)
                        for part in prerelease.split("."))
    return major, minor, patch, pre_key

def satisfies(version, constraint):
    current = version_key(version)
    if not isinstance(constraint, str) or not constraint.strip():
        raise ValueError("dependency range must be a non-empty string")
    for raw in constraint.split(","):
        term = raw.strip()
        if term in ("", "*"):
            continue
        match = re.fullmatch(r"(\^|~|>=|<=|>|<|==)?\s*(\d+\.\d+\.\d+(?:-[0-9A-Za-z.-]+)?)", term)
        if not match:
            raise ValueError("unsupported dependency range: " + term)
        operator, requested = match.groups()
        target = version_key(requested)
        operator = operator or "=="
        if operator == "^":
            major, minor, patch = target[:3]
            upper = (major+1, 0, 0) if major else ((0, minor+1, 0) if minor else (0, 0, patch+1))
            if not (current >= target and current < (*upper, ((2, ""),))):
                return False
        elif operator == "~":
            major, minor, patch = target[:3]
            if not (current >= target and current < (major, minor+1, 0, ((2, ""),))):
                return False
        elif operator == "==" and current != target:
            return False
        elif operator == ">=" and current < target:
            return False
        elif operator == "<=" and current > target:
            return False
        elif operator == ">" and current <= target:
            return False
        elif operator == "<" and current >= target:
            return False
    return True

def resolve_dependencies(index, root_name, root_version):
    packages = index.get("packages")
    if not isinstance(packages, dict):
        raise ValueError("registry index has no packages object")
    selected = {}

    def search(chosen, constraints):
        for name, version in chosen.items():
            if not all(satisfies(version, item) for item in constraints.get(name, [])):
                return None
        pending = sorted(name for name in constraints if name not in chosen)
        if not pending:
            return chosen
        name = pending[0]
        package = packages.get(name)
        if not isinstance(package, dict) or not isinstance(package.get("versions"), dict):
            return None
        candidates = []
        for version, metadata in package["versions"].items():
            try:
                version_key(version)
                if all(satisfies(version, item) for item in constraints[name]):
                    candidates.append(version)
            except ValueError:
                continue
        for version in sorted(candidates, key=version_key, reverse=True):
            metadata = package["versions"][version]
            dependencies = metadata.get("dependencies", {})
            if not isinstance(dependencies, dict):
                raise ValueError("dependencies for %s@%s must be an object" % (name, version))
            next_constraints = {key:list(value) for key,value in constraints.items()}
            valid = True
            for dep_name, dep_range in dependencies.items():
                valid_name(dep_name)
                if not isinstance(dep_range, str):
                    raise ValueError("dependency range for %s must be a string" % dep_name)
                next_constraints.setdefault(dep_name, []).append(dep_range)
                if dep_name in chosen and not satisfies(chosen[dep_name], dep_range):
                    valid = False
                    break
            if not valid:
                continue
            next_chosen = dict(chosen)
            next_chosen[name] = version
            result = search(next_chosen, next_constraints)
            if result is not None:
                return result
        return None

    solution = search({}, {valid_name(root_name): ["=="+valid_version(root_version)]})
    if solution is None:
        raise ValueError("dependency constraints cannot be resolved for %s@%s" % (root_name, root_version))
    return dict(sorted(solution.items()))

def install_resolution(root, index, resolution, lock, index_reference, root_name, root_version, force=False):
    """Download, verify and stage the complete resolved dependency graph before changing installs."""
    root = Path(root).resolve()
    private_root = root / ".taviq"
    package_root = private_root / "packages"
    if private_root.is_symlink() or package_root.is_symlink():
        raise ValueError("refusing to install through a symlinked TAVIQ package directory")
    package_root.mkdir(parents=True, exist_ok=True)
    package_root.resolve().relative_to(root)

    packages_index = index.get("packages", {})
    lock_packages = lock.setdefault("packages", {})
    if not isinstance(packages_index, dict) or not isinstance(lock_packages, dict):
        raise ValueError("registry packages and lock packages must be objects")
    prepared = []
    already_installed = []
    canonical_index = json.dumps(index, sort_keys=True, separators=(",", ":")).encode("utf-8")
    registry_sha256 = hashlib.sha256(canonical_index).hexdigest()

    # Stage and verify every missing package first; a failed download leaves existing installs untouched.
    with tempfile.TemporaryDirectory(prefix="taviq-pkg-transaction-", dir=package_root) as temporary:
        transaction = Path(temporary)
        staging_root = transaction / "staged"
        archives_root = transaction / "archives"
        backups_root = transaction / "backups"

        for package_name, package_version in sorted(resolution.items()):
            package_name = valid_name(package_name)
            package_version = valid_version(package_version)
            entry = packages_index.get(package_name, {}).get("versions", {}).get(package_version)
            if not isinstance(entry, dict):
                raise ValueError("package/version not found in registry index: %s@%s" %
                                 (package_name, package_version))
            url = entry.get("url")
            raw_digest = entry.get("sha256", "")
            expected = raw_digest.lower() if isinstance(raw_digest, str) else ""
            if not isinstance(url, str) or not re.fullmatch(r"[0-9a-f]{64}", expected):
                raise ValueError("registry entry must supply an HTTPS archive URL and SHA-256 for %s@%s" %
                                 (package_name, package_version))

            package_directory = package_root / package_name
            if package_directory.is_symlink():
                raise ValueError("refusing to install through a package symlink: " + package_name)
            destination = package_directory / package_version
            if destination.is_symlink():
                raise ValueError("refusing to replace a symlinked package directory: %s@%s" %
                                 (package_name, package_version))
            installed_versions = lock_packages.get(package_name, {})
            if not isinstance(installed_versions, dict):
                raise ValueError("invalid lock entries for package " + package_name)
            existing = installed_versions.get(package_version)
            if destination.is_dir() and isinstance(existing, dict) and existing.get("sha256") == expected and not force:
                already_installed.append(package_name + "@" + package_version)
                continue
            if destination.exists() and not force:
                raise ValueError("package directory already exists without a matching lock entry: %s@%s; pass --force to replace it" %
                                 (package_name, package_version))

            archive = archives_root / package_name / (package_version + ".archive")
            archive.parent.mkdir(parents=True, exist_ok=True)
            download(url, archive)
            if digest(archive) != expected:
                raise ValueError("archive SHA-256 mismatch for %s@%s" %
                                 (package_name, package_version))
            staged = staging_root / package_name / package_version
            extract_archive(archive, staged)
            prepared.append((package_name, package_version, url, expected, entry, staged, destination))

        # Commit directory renames as one recoverable transaction.
        backups = []
        installed_now = []
        try:
            for package_name, package_version, url, expected, entry, staged, destination in prepared:
                destination.parent.mkdir(parents=True, exist_ok=True)
                if destination.exists():
                    backup = backups_root / package_name / package_version
                    backup.parent.mkdir(parents=True, exist_ok=True)
                    os.replace(destination, backup)
                    backups.append((destination, backup))
                os.replace(staged, destination)
                installed_now.append(destination)

            for package_name, package_version, url, expected, entry, staged, destination in prepared:
                dependencies = entry.get("dependencies", {})
                if not isinstance(dependencies, dict):
                    raise ValueError("dependencies for %s@%s must be an object" %
                                     (package_name, package_version))
                lock_packages.setdefault(package_name, {})[package_version] = {
                    "sha256": expected,
                    "source": url,
                    "path": str(destination.relative_to(root)),
                    "dependencies": dict(sorted(dependencies.items())),
                    "registry_sha256": registry_sha256,
                }

            lock["resolution"] = {
                "root": {"name": valid_name(root_name), "version": valid_version(root_version)},
                "packages": dict(sorted(resolution.items())),
                "registry_sha256": registry_sha256,
                "registry": index_reference,
            }
            lock_path = root / "taviq.lock"
            temporary_lock = None
            try:
                with tempfile.NamedTemporaryFile(
                    mode="w", encoding="utf-8", dir=root,
                    prefix=".taviq-lock-", suffix=".part", delete=False
                ) as stream:
                    temporary_lock = Path(stream.name)
                    json.dump(lock, stream, indent=2, sort_keys=True)
                    stream.write("\n")
                os.replace(temporary_lock, lock_path)
                temporary_lock = None
            finally:
                if temporary_lock is not None:
                    temporary_lock.unlink(missing_ok=True)
        except Exception:
            for destination in reversed(installed_now):
                if destination.exists() and not destination.is_symlink():
                    shutil.rmtree(destination)
            for destination, backup in reversed(backups):
                if backup.exists():
                    destination.parent.mkdir(parents=True, exist_ok=True)
                    os.replace(backup, destination)
            raise

    return [package + "@" + package_version for package, package_version in sorted(resolution.items())
            if package + "@" + package_version not in already_installed], already_installed


def verify_index_signature(index_reference, signature_reference, public_key):
    if not public_key or not signature_reference:
        raise ValueError("a public key and detached signature are required")
    def read_reference(reference, limit):
        parsed = urllib.parse.urlparse(reference)
        if parsed.scheme == "https":
            req = urllib.request.Request(reference, headers={"User-Agent":"TAVIQ-Package-Manager/0.2"})
            with urllib.request.urlopen(req, timeout=30) as response:
                if urllib.parse.urlparse(response.geturl()).scheme != "https":
                    raise ValueError("signature URLs must not redirect away from HTTPS")
                data = response.read(limit+1)
            if len(data) > limit:
                raise ValueError("signed registry object exceeds size limit")
            return data
        return Path(reference).expanduser().read_bytes()
    try:
        signed_data = read_reference(index_reference, 8*1024*1024)
        signature = read_reference(signature_reference, 64*1024)
        key_path = Path(public_key).expanduser().resolve()
        if not key_path.is_file():
            raise ValueError("public key file does not exist")
        with tempfile.TemporaryDirectory(prefix="taviq-registry-verify-") as directory:
            root = Path(directory)
            data_path = root/"index.json"
            signature_path = root/"index.sig"
            data_path.write_bytes(signed_data)
            signature_path.write_bytes(signature)
            command = ["openssl","pkeyutl","-verify","-pubin","-inkey",str(key_path),
                       "-sigfile",str(signature_path),"-rawin","-in",str(data_path)]
            result = subprocess.run(command,capture_output=True,text=True,timeout=30,check=False)
            if result.returncode != 0:
                raise ValueError("registry Ed25519 signature verification failed")
        return json.loads(signed_data.decode("utf-8"))
    except FileNotFoundError as exc:
        raise ValueError("OpenSSL is required to verify signed registries") from exc

def main():
    p = argparse.ArgumentParser(description="TAVIQ package manager")
    sub = p.add_subparsers(dest="command", required=True)
    ins = sub.add_parser("install")
    ins.add_argument("name"); ins.add_argument("--version", required=True)
    ins.add_argument("--index", required=True, help="HTTPS index URL or local JSON index")
    ins.add_argument("--root", default="."); ins.add_argument("--force", action="store_true")
    ins.add_argument("--require-signature", action="store_true")
    ins.add_argument("--public-key"); ins.add_argument("--signature")
    rem = sub.add_parser("remove"); rem.add_argument("name"); rem.add_argument("--version")
    rem.add_argument("--root", default=".")
    ls = sub.add_parser("list"); ls.add_argument("--root", default=".")
    resolve = sub.add_parser("resolve", help="resolve semantic-version dependencies and write taviq.lock")
    resolve.add_argument("name"); resolve.add_argument("--version", required=True)
    resolve.add_argument("--index", required=True); resolve.add_argument("--root", default=".")
    resolve.add_argument("--require-signature", action="store_true")
    resolve.add_argument("--public-key"); resolve.add_argument("--signature")
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
        if args.command == "resolve":
            if args.require_signature:
                index = verify_index_signature(args.index,args.signature,args.public_key)
            else:
                index = load_index(args.index)
            resolution = resolve_dependencies(index,name,version)
            canonical_index = json.dumps(index,sort_keys=True,separators=(",",":")).encode("utf-8")
            lock["resolution"] = {"root":{"name":name,"version":version},
                                  "packages":resolution,
                                  "registry_sha256":hashlib.sha256(canonical_index).hexdigest()}
            tmp=lock_path.with_suffix(".lock.part")
            tmp.write_text(json.dumps(lock,indent=2,sort_keys=True),encoding="utf-8")
            os.replace(tmp,lock_path)
            for package, resolved_version in resolution.items():
                print(package+"@"+resolved_version)
            return 0
        installed, already_installed = install_resolution(
            root, index, resolve_dependencies(index, name, version), lock, args.index,
            name, version, args.force
        )
        for package in already_installed:
            print("already installed", package)
        for package in installed:
            print("installed", package, "without executing package scripts")
        return 0
    except (OSError, ValueError, KeyError, tarfile.TarError, zipfile.BadZipFile,
            urllib.error.URLError, json.JSONDecodeError) as exc:
        print("taviq-pkg:", exc, file=sys.stderr); return 1

if __name__ == "__main__":
    raise SystemExit(main())
