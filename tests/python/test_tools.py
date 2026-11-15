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

    def test_zip_extraction(self):
        with tempfile.TemporaryDirectory() as d:
            root = Path(d); archive = root / "package.zip"
            with zipfile.ZipFile(archive, "w") as z: z.writestr("pkg/hello.txt", "ok")
            dest = root / "out"; pkg.extract_archive(archive, dest)
            self.assertEqual((dest / "pkg" / "hello.txt").read_text(), "ok")

if __name__ == "__main__": unittest.main()
