import importlib.util
from pathlib import Path
import tempfile
import unittest
import zipfile

ROOT = Path(__file__).resolve().parents[2]

def load_tool(name, path):
    spec = importlib.util.spec_from_file_location(name, ROOT / path)
    module = importlib.util.module_from_spec(spec); spec.loader.exec_module(module)
    return module

hub = load_tool("taviq_hub", "tools/taviq_hub.py")
pkg = load_tool("taviq_pkg", "tools/taviq_pkg.py")
ai = load_tool("taviq_ai", "ai/taviq_ai.py")
dependency_audit = load_tool("taviq_dependency_audit", "tools/check_dependencies.py")

class ToolSafetyTests(unittest.TestCase):
    def test_model_id_and_allowlist(self):
        self.assertEqual(hub.safe_model_id("org/model"), "org/model")
        for value in ("../model", "org/../model", "org/.", "/model", "org//model", "a\\b"):
            with self.subTest(value=value), self.assertRaises(ValueError):
                hub.safe_model_id(value)
        self.assertEqual(str(hub.safe_file("weights/model.gguf")), "weights/model.gguf")
        with self.assertRaises(ValueError): hub.safe_file("../../evil.py")
        with self.assertRaises(ValueError): hub.safe_file("weights/evil.pkl")
        with self.assertRaises(ValueError): hub.safe_file("..\\..\\escape.gguf")
        with self.assertRaises(ValueError): hub.safe_file("C:\\escape.gguf")

    def test_model_download_budget_is_bounded(self):
        self.assertEqual(hub.validate_max_total(1024), 1024)
        for value in (0, -1, hub.MAX_TOTAL + 1, 1.5):
            with self.subTest(value=value), self.assertRaises(ValueError):
                hub.validate_max_total(value)

    def test_ai_endpoint_is_loopback_by_default(self):
        self.assertEqual(ai.validate_endpoint("http://127.0.0.1:8080"), "http://127.0.0.1:8080/completion")
        self.assertEqual(ai.validate_endpoint("http://localhost:8080/"), "http://localhost:8080/completion")
        for endpoint in ("ftp://localhost", "http://user:pass@localhost", "http://localhost?q=1", "https://example.com"):
            with self.subTest(endpoint=endpoint), self.assertRaises(ValueError):
                ai.validate_endpoint(endpoint)

    def test_ai_generation_bounds(self):
        ai.validate_generation("hello", 64, 0.7)
        for temperature in (-0.1, 2.01, float("nan"), float("inf")):
            with self.subTest(temperature=temperature), self.assertRaises(ValueError):
                ai.validate_generation("hello", 64, temperature)
        with self.assertRaises(ValueError): ai.validate_generation("", 64, 0.7)
        with self.assertRaises(ValueError): ai.validate_generation("hello", 9000, 0.7)

    def test_dependency_policy(self):
        self.assertEqual(dependency_audit.audit(ROOT), [])

    def test_package_version_rejects_traversal(self):
        self.assertEqual(pkg.valid_version("1.2.3-beta.1+build"), "1.2.3-beta.1+build")
        for value in ("../escape", "..\\escape", "/root", "C:escape", ""):
            with self.subTest(value=value), self.assertRaises(ValueError):
                pkg.valid_version(value)

    def test_package_path_rejects_traversal(self):
        with tempfile.TemporaryDirectory() as d:
            with self.assertRaises(ValueError): pkg.safe_target(Path(d), "../escape.txt")
            with self.assertRaises(ValueError): pkg.safe_target(Path(d), "subdir\\..\\escape.txt")


    def test_semver_dependency_resolution(self):
        index = {"packages": {
            "app": {"versions": {"1.0.0": {"dependencies": {"lib": "^1.0.0", "codec": "~2.1.0"}}}},
            "lib": {"versions": {
                "1.0.0": {"dependencies": {}}, "1.2.0": {"dependencies": {}},
                "2.0.0": {"dependencies": {}}
            }},
            "codec": {"versions": {"2.1.0": {"dependencies": {}}, "2.2.0": {"dependencies": {}}}}
        }}
        resolved = pkg.resolve_dependencies(index, "app", "1.0.0")
        self.assertEqual(resolved, {"app":"1.0.0", "codec":"2.1.0", "lib":"1.2.0"})

    def test_semver_dependency_conflict_is_rejected(self):
        index = {"packages": {
            "app": {"versions": {"1.0.0": {"dependencies": {"left": "*", "right": "*"}}}},
            "left": {"versions": {"1.0.0": {"dependencies": {"shared": "^1.0.0"}}}},
            "right": {"versions": {"1.0.0": {"dependencies": {"shared": "^2.0.0"}}}},
            "shared": {"versions": {"1.5.0": {"dependencies": {}}, "2.0.0": {"dependencies": {}}}}
        }}
        with self.assertRaises(ValueError):
            pkg.resolve_dependencies(index, "app", "1.0.0")

    def test_semver_dependency_resolution(self):
        index = {"packages": {
            "app": {"versions": {"1.0.0": {"dependencies": {"lib": "^1.0.0", "codec": "~2.1.0"}}}},
            "lib": {"versions": {
                "1.0.0": {"dependencies": {}}, "1.2.0": {"dependencies": {}},
                "2.0.0": {"dependencies": {}}
            }},
            "codec": {"versions": {"2.1.0": {"dependencies": {}}, "2.2.0": {"dependencies": {}}}}
        }}
        self.assertEqual(pkg.resolve_dependencies(index,"app","1.0.0"),
                         {"app":"1.0.0","codec":"2.1.0","lib":"1.2.0"})

    def test_semver_dependency_conflict_is_rejected(self):
        index = {"packages": {
            "app": {"versions": {"1.0.0": {"dependencies": {"left": "*", "right": "*"}}}},
            "left": {"versions": {"1.0.0": {"dependencies": {"shared": "^1.0.0"}}}},
            "right": {"versions": {"1.0.0": {"dependencies": {"shared": "^2.0.0"}}}},
            "shared": {"versions": {"1.5.0": {"dependencies": {}}, "2.0.0": {"dependencies": {}}}}
        }}
        with self.assertRaises(ValueError):
            pkg.resolve_dependencies(index,"app","1.0.0")

    def test_install_resolved_dependencies_transactionally(self):
        import hashlib
        import json
        import shutil
        from unittest.mock import patch

        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            archives = {}
            index = {"packages": {}}
            payloads = {
                "app": ("app.bin", b"application"),
                "lib": ("lib.bin", b"library"),
                "codec": ("codec.bin", b"codec"),
            }
            dependencies = {
                "app": {"lib": "^1.0.0"},
                "lib": {"codec": "^2.0.0"},
                "codec": {},
            }
            for name, (filename, payload) in payloads.items():
                archive = root / (name + ".zip")
                with zipfile.ZipFile(archive, "w") as bundle:
                    bundle.writestr("package/" + filename, payload)
                url = "https://registry.invalid/" + name + ".zip"
                archives[url] = archive
                index["packages"][name] = {"versions": {
                    {"app": "1.0.0", "lib": "1.1.0", "codec": "2.0.0"}[name]: {
                        "url": url,
                        "sha256": hashlib.sha256(archive.read_bytes()).hexdigest(),
                        "dependencies": dependencies[name],
                    }
                }}

            def fake_download(url, destination):
                shutil.copyfile(archives[url], destination)

            resolved = pkg.resolve_dependencies(index, "app", "1.0.0")
            with patch.object(pkg, "load_index", return_value=index), \
                 patch.object(pkg, "download", side_effect=fake_download) as mocked, \
                 patch.object(pkg.sys, "argv", [
                     "taviq-pkg", "install", "app", "--version", "1.0.0",
                     "--index", "https://registry.invalid/index.json", "--root", str(root)
                 ]):
                self.assertEqual(pkg.main(), 0)
                self.assertEqual(mocked.call_count, 3)
            self.assertEqual(
                sorted(path.name + "@" + path.parent.name
                       for path in (root / ".taviq" / "packages").glob("*/*")),
                ["1.0.0@app", "1.1.0@lib", "2.0.0@codec"]
            )
            for name, (filename, payload) in payloads.items():
                installed_file = root / ".taviq" / "packages" / name / {
                    "app": "1.0.0", "lib": "1.1.0", "codec": "2.0.0"
                }[name] / "package" / filename
                self.assertEqual(installed_file.read_bytes(), payload)

            lock_path = root / "taviq.lock"
            persisted = json.loads(lock_path.read_text(encoding="utf-8"))
            self.assertEqual(persisted["resolution"]["packages"], resolved)
            self.assertEqual(persisted["resolution"]["root"], {"name": "app", "version": "1.0.0"})
            with patch.object(pkg, "download", side_effect=AssertionError("installed package was downloaded")):
                installed_again, existing_again = pkg.install_resolution(
                    root, index, resolved, persisted, "https://registry.invalid/index.json",
                    "app", "1.0.0"
                )
            self.assertEqual(installed_again, [])
            self.assertEqual(len(existing_again), 3)

    def test_install_resolution_download_failure_changes_nothing(self):
        import hashlib
        import shutil
        from unittest.mock import patch

        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            archive = root / "app.zip"
            with zipfile.ZipFile(archive, "w") as bundle:
                bundle.writestr("app.txt", "ready")
            index = {"packages": {
                "app": {"versions": {"1.0.0": {
                    "url": "https://registry.invalid/app.zip",
                    "sha256": hashlib.sha256(archive.read_bytes()).hexdigest(),
                    "dependencies": {"lib": "^1.0.0"},
                }}},
                "lib": {"versions": {"1.0.0": {
                    "url": "https://registry.invalid/lib.zip",
                    "sha256": "0" * 64,
                    "dependencies": {},
                }}},
            }}
            def fake_download(url, destination):
                if url.endswith("/app.zip"):
                    shutil.copyfile(archive, destination)
                else:
                    destination.write_bytes(b"corrupt archive")
            lock = {"format": 1, "packages": {}}
            resolved = pkg.resolve_dependencies(index, "app", "1.0.0")
            with patch.object(pkg, "download", side_effect=fake_download):
                with self.assertRaisesRegex(ValueError, "SHA-256 mismatch"):
                    pkg.install_resolution(
                        root, index, resolved, lock, "https://registry.invalid/index.json",
                        "app", "1.0.0"
                    )
            self.assertFalse((root / ".taviq" / "packages" / "app" / "1.0.0").exists())
            self.assertFalse((root / ".taviq" / "packages" / "lib" / "1.0.0").exists())
            self.assertFalse((root / "taviq.lock").exists())

    def test_zip_extraction(self):
        with tempfile.TemporaryDirectory() as d:
            root = Path(d); archive = root / "package.zip"
            with zipfile.ZipFile(archive, "w") as z: z.writestr("pkg/hello.txt", "ok")
            dest = root / "out"; pkg.extract_archive(archive, dest)
            self.assertEqual((dest / "pkg" / "hello.txt").read_text(), "ok")

if __name__ == "__main__": unittest.main()
