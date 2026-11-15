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

class ToolSafetyTests(unittest.TestCase):
    def test_model_id_and_allowlist(self):
        self.assertEqual(hub.safe_model_id("org/model"), "org/model")
        for value in ("../model", "org/../model", "org/.", "/model", "org//model", "a\\b"):
            with self.subTest(value=value), self.assertRaises(ValueError):
                hub.safe_model_id(value)
        self.assertEqual(str(hub.safe_file("weights/model.gguf")), "weights/model.gguf")
        with self.assertRaises(ValueError): hub.safe_file("../../evil.py")
        with self.assertRaises(ValueError): hub.safe_file("weights/evil.pkl")

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
