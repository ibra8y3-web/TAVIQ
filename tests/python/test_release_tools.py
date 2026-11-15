import importlib.util
from pathlib import Path
import tempfile
import unittest

ROOT=Path(__file__).resolve().parents[2]
def load(name,path):
    spec=importlib.util.spec_from_file_location(name,ROOT/"tools"/path)
    module=importlib.util.module_from_spec(spec);spec.loader.exec_module(module);return module
sbom=load("taviq_sbom","taviq_sbom.py")
release=load("taviq_release","taviq_release.py")

class ReleaseToolsTests(unittest.TestCase):
    def test_sbom_is_deterministic(self):
        first=sbom.generate(ROOT,"test-version",1700000000)
        second=sbom.generate(ROOT,"test-version",1700000000)
        self.assertEqual(first,second)
        self.assertEqual(first["bomFormat"],"CycloneDX")
        self.assertTrue(first["components"])
        self.assertTrue(all(x["hashes"][0]["alg"]=="SHA-256" for x in first["components"]))

    def test_archives_are_byte_for_byte_reproducible(self):
        with tempfile.TemporaryDirectory() as directory:
            root=Path(directory)
            binary=root/"taviq";binary.write_bytes(b"test executable")
            bom=root/"sbom.json";bom.write_text('{"bomFormat":"CycloneDX"}',encoding="utf-8")
            first=release.build(binary,bom,root/"first","1.2.3","linux-x86_64",epoch=1700000000)
            second=release.build(binary,bom,root/"second","1.2.3","linux-x86_64",epoch=1700000000)
            self.assertEqual([p.read_bytes() for p in first],[p.read_bytes() for p in second])

if __name__=="__main__":
    unittest.main()
