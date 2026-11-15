#!/usr/bin/env python3
"""Minimal stdio Language Server Protocol implementation for TAVIQ."""
import json
import os
from pathlib import Path
import re
import subprocess
import sys
import tempfile

MAX_MESSAGE = 8 * 1024 * 1024
KEYWORDS = [
    ("flow", "program"), ("main", "entry flow"), ("let", "immutable local"),
    ("var", "mutable local"), ("if", "conditional branch"), ("else", "alternate branch"),
    ("while", "loop"), ("display", "print a value"), ("return", "return from a function"),
    ("true", "Boolean literal"), ("false", "Boolean literal"),
    ("and", "short-circuit Boolean AND"), ("or", "short-circuit Boolean OR"),
    ("not", "Boolean negation"), ("fn", "function declaration")
]
BUILTINS = [
    ("len", "len(string) -> number"), ("sqrt", "sqrt(number) -> number"),
    ("abs", "abs(number) -> number")
]
WORD = re.compile(r"[A-Za-z_][A-Za-z0-9_]*")

def word_at(text, line, character):
    lines = text.splitlines()
    if line < 0 or line >= len(lines):
        return ""
    row = lines[line]
    pos = min(max(character, 0), len(row))
    for match in WORD.finditer(row):
        if match.start() <= pos <= match.end():
            return match.group(0)
    return ""

def diagnostics_for(text, compiler=None):
    compiler = compiler or os.environ.get("TAVIQ_COMPILER", "")
    if not compiler:
        return []
    executable = Path(compiler).expanduser().resolve()
    if not executable.is_file():
        return [{"range":{"start":{"line":0,"character":0},"end":{"line":0,"character":1}},
                 "severity":1,"source":"taviq","message":"Compiler executable not found: "+str(executable)}]
    with tempfile.TemporaryDirectory(prefix="taviq-lsp-") as directory:
        source = Path(directory) / "document.taviq"
        source.write_text(text, encoding="utf-8")
        try:
            result = subprocess.run([str(executable), "check", str(source)],
                                    capture_output=True, text=True, timeout=10, check=False)
        except (OSError, subprocess.TimeoutExpired) as exc:
            return [{"range":{"start":{"line":0,"character":0},"end":{"line":0,"character":1}},
                     "severity":1,"source":"taviq","message":"Compiler invocation failed: "+str(exc)}]
    if result.returncode == 0:
        return []
    diagnostics = []
    for line in (result.stderr + "\n" + result.stdout).splitlines():
        match = re.match(r"(?:Line|line)\s+(\d+):\s*(.*)", line)
        if match:
            row = max(0, int(match.group(1))-1)
            diagnostics.append({
                "range":{"start":{"line":row,"character":0},"end":{"line":row,"character":1000}},
                "severity":1,"source":"taviq","message":match.group(2) or "Syntax or type error"
            })
    if not diagnostics:
        diagnostics.append({"range":{"start":{"line":0,"character":0},"end":{"line":0,"character":1}},
                            "severity":1,"source":"taviq",
                            "message":(result.stderr or result.stdout or "TAVIQ compilation failed")[:2048]})
    return diagnostics

def completion_items():
    items = [{"label":name,"kind":14,"detail":detail} for name,detail in KEYWORDS]
    items.extend({"label":name,"kind":3,"detail":detail} for name,detail in BUILTINS)
    return items

class Server:
    def __init__(self, compiler=None):
        self.compiler = compiler or os.environ.get("TAVIQ_COMPILER", "")
        self.documents = {}
        self.shutdown_requested = False

    def dispatch(self, message):
        method = message.get("method")
        params = message.get("params") or {}
        request_id = message.get("id")
        if method == "initialize":
            return {"capabilities":{
                "textDocumentSync":1,
                "completionProvider":{"triggerCharacters":["."]},
                "hoverProvider":True
            },"serverInfo":{"name":"TAVIQ Language Server","version":"0.4.0"}}
        if method == "initialized":
            return None
        if method == "shutdown":
            self.shutdown_requested = True
            return None
        if method == "exit":
            raise EOFError
        if method in ("textDocument/didOpen","textDocument/didChange","textDocument/didSave"):
            document = params.get("textDocument", {})
            uri = document.get("uri", "")
            if method == "textDocument/didOpen":
                text = document.get("text", "")
            elif method == "textDocument/didChange":
                changes = params.get("contentChanges") or []
                text = changes[-1].get("text", self.documents.get(uri, "")) if changes else self.documents.get(uri, "")
            else:
                text = self.documents.get(uri, "")
            if uri and isinstance(text, str):
                self.documents[uri] = text
                return {"notification":"textDocument/publishDiagnostics",
                        "params":{"uri":uri,"diagnostics":diagnostics_for(text,self.compiler)}}
            return None
        if method == "textDocument/completion":
            return {"isIncomplete":False,"items":completion_items()}
        if method == "textDocument/hover":
            doc = params.get("textDocument", {})
            uri = doc.get("uri", "")
            pos = params.get("position", {})
            word = word_at(self.documents.get(uri, ""), int(pos.get("line",0)),
                           int(pos.get("character",0)))
            details = dict(KEYWORDS + BUILTINS)
            if word in details:
                return {"contents":{"kind":"markdown","value":"**"+word+"** — "+details[word]}}
            return None
        if request_id is not None:
            return {"error":{"code":-32601,"message":"Method not found: "+str(method)}}
        return None

def read_message(stream):
    headers = {}
    while True:
        line = stream.readline()
        if not line:
            return None
        if len(line) > 16384:
            raise ValueError("LSP header line exceeds limit")
        if line in (b"\r\n", b"\n"):
            break
        key, sep, value = line.decode("ascii", "strict").partition(":")
        if not sep:
            raise ValueError("invalid LSP header")
        headers[key.lower()] = value.strip()
    length = int(headers.get("content-length", "0"))
    if length <= 0 or length > MAX_MESSAGE:
        raise ValueError("invalid LSP Content-Length")
    payload = stream.read(length)
    if len(payload) != length:
        raise EOFError
    return json.loads(payload.decode("utf-8"))

def write_message(stream, message):
    payload = json.dumps(message, separators=(",",":"), ensure_ascii=False).encode("utf-8")
    stream.write(("Content-Length: %d\r\n\r\n" % len(payload)).encode("ascii"))
    stream.write(payload)
    stream.flush()

def main():
    compiler = sys.argv[1] if len(sys.argv) > 1 else None
    server = Server(compiler)
    inp, out = sys.stdin.buffer, sys.stdout.buffer
    try:
        while True:
            message = read_message(inp)
            if message is None:
                break
            try:
                result = server.dispatch(message)
                request_id = message.get("id")
                if request_id is not None:
                    response = {"jsonrpc":"2.0","id":request_id}
                    if isinstance(result, dict) and "error" in result:
                        response.update(result)
                    else:
                        response["result"] = result
                    write_message(out, response)
                elif isinstance(result, dict) and "notification" in result:
                    write_message(out, {"jsonrpc":"2.0","method":result["notification"],"params":result["params"]})
            except EOFError:
                break
    except (ValueError, UnicodeDecodeError, json.JSONDecodeError) as exc:
        print("TAVIQ LSP protocol error: "+str(exc), file=sys.stderr)
        return 1
    return 0

if __name__ == "__main__":
    raise SystemExit(main())
