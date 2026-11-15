#!/usr/bin/env python3
"""Loopback-only TAVIQ project editor integrated with compiler diagnostics and QVM tools."""
import argparse
import importlib.util
import json
from pathlib import Path
import re
import subprocess
import sys
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from urllib.parse import parse_qs, urlparse

MAX_SOURCE_BYTES = 2_000_000
MAX_PROJECT_FILES = 200
EXCLUDED_DIRS = {".git", ".taviq", "build", "build-android", "dist", ".venv",
                 "__pycache__", "node_modules"}

PAGE = """<!doctype html><html><head><meta charset="utf-8"><meta name="viewport" content="width=device-width">
<title>TAVIQ Studio</title><style>
body{font:15px system-ui;background:#10141d;color:#e9eef7;margin:0}header{padding:14px 20px;background:#1c2534;display:flex;gap:8px;align-items:center;flex-wrap:wrap}
main{display:grid;grid-template-columns:240px 1fr;gap:12px;padding:12px;min-height:calc(100vh - 80px);box-sizing:border-box}
aside,section{background:#1a2230;border-radius:10px;padding:12px;overflow:auto}button{background:#3978c7;border:0;color:white;padding:9px 14px;border-radius:7px}
textarea{width:100%;height:65vh;box-sizing:border-box;background:#0c111a;color:#e9eef7;border:1px solid #3b4a62;border-radius:8px;padding:12px;font:14px ui-monospace,monospace}
pre{white-space:pre-wrap;background:#0c111a;padding:12px;border-radius:8px;min-height:100px}li{padding:7px;cursor:pointer;overflow-wrap:anywhere}li:hover{background:#2a3548}
@media(max-width:700px){main{grid-template-columns:1fr}textarea{height:50vh}}
</style></head><body><header><b>TAVIQ Studio</b><span>Local Project IDE</span>
<button id="new">New</button><button id="save">Save</button><button id="check">Check</button>
<button id="format">Format</button><button id="hints">Completions</button>
<button id="debug">Debug</button><button id="run">Run</button></header>
<main><aside><h3>Project files</h3><ul id="files"></ul></aside><section>
<div id="current">Select or create a .taviq file</div>
<textarea id="editor" spellcheck="false"></textarea><h3>Diagnostics / Output</h3><pre id="output"></pre>
</section></main>
<script>
let currentId="";const editor=document.querySelector("#editor"),out=document.querySelector("#output");
async function api(url,method="GET",body=null){
 const options={method,headers:{"Content-Type":"application/json"}};
 if(body!==null)options.body=JSON.stringify(body);
 const response=await fetch(url,options);const data=await response.json();
 if(!response.ok)throw new Error(data.error||"Studio request failed");
 return data;
}
async function refresh(){
 const data=await api("/api/files");const ul=document.querySelector("#files");ul.textContent="";
 for(const file of data.files){const li=document.createElement("li");li.textContent=file.path;li.onclick=()=>openFile(file.id,file.path);ul.appendChild(li)}
}
async function openFile(id,path){
 currentId=id;document.querySelector("#current").textContent=path;
 const data=await api("/api/file?id="+encodeURIComponent(id));editor.value=data.content||"";out.textContent="";
}
async function save(){
 if(!currentId)throw new Error("Create or select a .taviq file first");
 await api("/api/file","PUT",{id:currentId,content:editor.value});out.textContent="Saved";
}
async function action(name){
 await save();
 const data=await api("/api/"+name,"POST",{id:currentId});
 out.textContent=(data.stdout||"")+(data.stderr||"")+(data.error||"")+
   (data.diagnostics?JSON.stringify(data.diagnostics,null,2):"");
 if(name==="format"){await openFile(currentId,document.querySelector("#current").textContent);await refresh()}
}
document.querySelector("#new").onclick=async()=>{
 try{const data=await api("/api/project/create","POST",{});await refresh();await openFile(data.id,data.path)}
 catch(e){out.textContent=e.message}
};
document.querySelector("#save").onclick=()=>save().catch(e=>out.textContent=e.message);
document.querySelector("#check").onclick=()=>action("check").catch(e=>out.textContent=e.message);
document.querySelector("#format").onclick=()=>action("format").catch(e=>out.textContent=e.message);
document.querySelector("#debug").onclick=()=>action("debug").catch(e=>out.textContent=e.message);
document.querySelector("#run").onclick=()=>action("run").catch(e=>out.textContent=e.message);
document.querySelector("#hints").onclick=async()=>{
 try{const data=await api("/api/completions");out.textContent=data.items.map(x=>x.label+" — "+x.detail).join("\\n")}
 catch(e){out.textContent=e.message}
};
refresh().catch(e=>out.textContent=e.message);
</script></body></html>"""

class Handler(BaseHTTPRequestHandler):
    root = Path(".").resolve()
    compiler = Path("build/taviq").resolve()
    python = sys.executable
    lsp_path = Path("tools/taviq_lsp.py")
    dev_path = Path("tools/taviq_dev.py")

    def log_message(self, *_):
        pass

    def send_json(self, code, data):
        raw = json.dumps(data, ensure_ascii=False).encode("utf-8")
        self.send_response(code)
        self.send_header("Content-Type", "application/json; charset=utf-8")
        self.send_header("Content-Length", str(len(raw)))
        self.send_header("X-Content-Type-Options", "nosniff")
        self.send_header("Cache-Control", "no-store")
        self.end_headers()
        self.wfile.write(raw)

    def allowed_origin(self, require_origin=False):
        host = urlparse("//" + self.headers.get("Host", "")).hostname
        if host not in ("127.0.0.1", "localhost"):
            return False
        origin = self.headers.get("Origin")
        if not origin:
            return not require_origin
        parsed = urlparse(origin)
        return parsed.scheme == "http" and parsed.hostname in ("127.0.0.1", "localhost")

    def project_files(self):
        found = []
        for candidate in self.root.rglob("*.taviq"):
            try:
                relative = candidate.relative_to(self.root)
                if candidate.is_symlink() or any(part in EXCLUDED_DIRS for part in relative.parts):
                    continue
                resolved = candidate.resolve(strict=True)
                resolved.relative_to(self.root)
                if not resolved.is_file() or resolved.stat().st_size > MAX_SOURCE_BYTES:
                    continue
                found.append((relative.as_posix(), resolved))
            except (OSError, ValueError):
                continue
        found.sort(key=lambda item: item[0])
        return found[:MAX_PROJECT_FILES]

    def project_file(self, identifier):
        if not isinstance(identifier, str) or not re.fullmatch(r"(?:0|[1-9][0-9]{0,5})", identifier):
            raise ValueError("invalid project file identifier")
        index = int(identifier)
        files = self.project_files()
        if index >= len(files):
            raise ValueError("project file does not exist")
        return files[index]

    def create_project_file(self):
        for number in range(1, 10001):
            filename = "untitled-%d.taviq" % number
            target = self.root / filename
            try:
                with target.open("x", encoding="utf-8") as stream:
                    stream.write('flow main {\n    display "Hello from TAVIQ";\n}\n')
                break
            except FileExistsError:
                continue
        else:
            raise RuntimeError("no free project filename is available")
        for index, item in enumerate(self.project_files()):
            if item[1] == target.resolve():
                return str(index), item[0]
        raise RuntimeError("created project file could not be indexed")

    def read_json(self, limit):
        length = int(self.headers.get("Content-Length", "0"))
        if length <= 0 or length > limit:
            raise ValueError("request body is empty or exceeds size limit")
        return json.loads(self.rfile.read(length))

    @classmethod
    def load_lsp(cls):
        spec = importlib.util.spec_from_file_location("taviq_studio_lsp", cls.root / cls.lsp_path)
        if spec is None or spec.loader is None:
            raise RuntimeError("TAVIQ LSP module could not be loaded")
        module = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(module)
        return module

    def do_GET(self):
        if not self.allowed_origin():
            return self.send_json(403, {"error": "host or origin rejected"})
        parsed = urlparse(self.path)
        if parsed.path == "/":
            body = PAGE.encode("utf-8")
            self.send_response(200)
            self.send_header("Content-Type", "text/html; charset=utf-8")
            self.send_header("Content-Length", str(len(body)))
            self.send_header("X-Content-Type-Options", "nosniff")
            self.send_header("Content-Security-Policy", "default-src 'self'; script-src 'unsafe-inline'; style-src 'unsafe-inline'")
            self.end_headers()
            self.wfile.write(body)
            return
        try:
            if parsed.path == "/api/files":
                files = [{"id": str(index), "path": name}
                         for index, (name, _) in enumerate(self.project_files())]
                self.send_json(200, {"files": files})
                return
            if parsed.path == "/api/file":
                identifier = parse_qs(parsed.query).get("id", [""])[0]
                name, target = self.project_file(identifier)
                content = target.read_text(encoding="utf-8")
                if len(content.encode("utf-8")) > MAX_SOURCE_BYTES:
                    raise ValueError("source file exceeds 2 MiB")
                self.send_json(200, {"path": name, "content": content})
                return
            if parsed.path == "/api/completions":
                module = self.load_lsp()
                self.send_json(200, {"items": module.completion_items()})
                return
            self.send_json(404, {"error": "not found"})
        except (OSError, ValueError, RuntimeError, json.JSONDecodeError) as exc:
            self.send_json(400, {"error": str(exc)})

    def do_PUT(self):
        if not self.allowed_origin(require_origin=True):
            return self.send_json(403, {"error": "host or origin rejected"})
        try:
            data = self.read_json(MAX_SOURCE_BYTES + 8192)
            _, target = self.project_file(data.get("id"))
            content = data.get("content")
            if not isinstance(content, str) or len(content.encode("utf-8")) > MAX_SOURCE_BYTES:
                raise ValueError("content must be text and no larger than 2 MiB")
            temporary = target.with_name(target.name + ".part")
            temporary.write_text(content, encoding="utf-8")
            temporary.replace(target)
            self.send_json(200, {"saved": True})
        except (OSError, ValueError, KeyError, TypeError, json.JSONDecodeError) as exc:
            self.send_json(400, {"error": str(exc)})

    def do_POST(self):
        if not self.allowed_origin(require_origin=True):
            return self.send_json(403, {"error": "host or origin rejected"})
        try:
            data = self.read_json(8192)
            route = urlparse(self.path).path
            if route == "/api/project/create":
                identifier, name = self.create_project_file()
                self.send_json(201, {"created": True, "id": identifier, "path": name})
                return
            name, target = self.project_file(data.get("id"))
            if not target.is_file():
                raise ValueError("project file does not exist")
            relative = target.relative_to(self.root).as_posix()
            if route == "/api/format":
                command = [self.python, str(self.root / self.dev_path), "format", relative]
            elif route == "/api/debug":
                command = [str(self.compiler), "debug", relative]
            elif route == "/api/check":
                command = [str(self.compiler), "check", relative]
            elif route == "/api/run":
                command = [str(self.compiler), "run", relative]
            else:
                return self.send_json(404, {"error": "unknown Studio action"})

            result = subprocess.run(command, cwd=self.root, capture_output=True,
                                    text=True, timeout=30, check=False, shell=False)
            self.send_json(200, {"returncode": result.returncode,
                                 "stdout": result.stdout[:100000],
                                 "stderr": result.stderr[:100000]})
        except (OSError, ValueError, KeyError, TypeError, json.JSONDecodeError,
                subprocess.TimeoutExpired) as exc:
            self.send_json(400, {"error": str(exc)})

def main():
    parser = argparse.ArgumentParser(description="Run the local TAVIQ Studio project editor")
    parser.add_argument("--root", default=".")
    parser.add_argument("--compiler", default="build/taviq")
    parser.add_argument("--port", type=int, default=8765)
    args = parser.parse_args()
    Handler.root = Path(args.root).resolve()
    compiler = Path(args.compiler)
    Handler.compiler = (Handler.root / compiler).resolve() if not compiler.is_absolute() else compiler.resolve()
    if not Handler.root.is_dir():
        parser.error("project root does not exist")
    try:
        Handler.compiler.relative_to(Handler.root)
    except ValueError:
        parser.error("compiler must be located inside the project root")
    if not Handler.compiler.is_file():
        parser.error("compiler executable not found: " + str(Handler.compiler))
    for tool in (Handler.root / Handler.lsp_path, Handler.root / Handler.dev_path):
        if not tool.is_file():
            parser.error("required Studio tool not found: " + str(tool))
    server = ThreadingHTTPServer(("127.0.0.1", args.port), Handler)
    print("TAVIQ Studio: http://127.0.0.1:%d/ (loopback only; Ctrl+C to stop)" % args.port)
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        pass
    finally:
        server.server_close()

if __name__ == "__main__":
    main()
