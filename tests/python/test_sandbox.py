import importlib.util
from pathlib import Path
import tempfile
import unittest

ROOT=Path(__file__).resolve().parents[2]
spec=importlib.util.spec_from_file_location("taviq_sandbox",ROOT/"tools"/"taviq_sandbox.py")
sandbox=importlib.util.module_from_spec(spec);spec.loader.exec_module(sandbox)

class SandboxTests(unittest.TestCase):
    def test_command_is_namespaced_and_network_is_denied_by_default(self):
        original=sandbox.shutil.which
        sandbox.shutil.which=lambda _: "/usr/bin/bwrap"
        try:
            with tempfile.TemporaryDirectory() as directory:
                command=sandbox.build_command("/usr/bin/env",["true"],directory)
                self.assertIn("--unshare-all",command)
                self.assertIn("--unshare-net",command)
                self.assertIn("--bind",command)
                readonly=sandbox.build_command("/usr/bin/env",[],directory,readonly=True)
                self.assertIn("--ro-bind",readonly)
                self.assertIn("--unshare-net",readonly)
                allowed=sandbox.build_command("/usr/bin/env",[],directory,network=True)
                self.assertNotIn("--unshare-net",allowed)
        finally:
            sandbox.shutil.which=original

    def test_missing_bwrap_fails_closed(self):
        original=sandbox.shutil.which
        sandbox.shutil.which=lambda _:None
        try:
            with tempfile.TemporaryDirectory() as directory:
                with self.assertRaises(RuntimeError):
                    sandbox.build_command("/bin/true",[],directory)
        finally:
            sandbox.shutil.which=original

if __name__=="__main__":unittest.main()
