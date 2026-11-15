import importlib.util
from pathlib import Path
import unittest

ROOT=Path(__file__).resolve().parents[2]
spec=importlib.util.spec_from_file_location("taviq_lsp",ROOT/"tools"/"taviq_lsp.py")
lsp=importlib.util.module_from_spec(spec);spec.loader.exec_module(lsp)

class LspTests(unittest.TestCase):
    def test_word_lookup(self):
        source="flow main { display sqrt(9); }"
        self.assertEqual(lsp.word_at(source,0,24),"sqrt")
        self.assertEqual(lsp.word_at(source,4,0),"")
    def test_completion_inventory(self):
        labels={item["label"] for item in lsp.completion_items()}
        self.assertTrue({"flow","if","while","fn","sqrt","len"}.issubset(labels))
    def test_hover(self):
        server=lsp.Server()
        server.documents["file:///demo.taviq"]="flow main { display sqrt(9); }"
        answer=server.dispatch({"jsonrpc":"2.0","id":1,"method":"textDocument/hover",
          "params":{"textDocument":{"uri":"file:///demo.taviq"},"position":{"line":0,"character":24}}})
        self.assertIn("sqrt(non-negative number)",answer["contents"]["value"])
    def test_diagnostics_without_compiler_are_non_fabricated(self):
        self.assertEqual(lsp.diagnostics_for("flow main {}",""),[])

if __name__=="__main__":unittest.main()
