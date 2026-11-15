#!/usr/bin/env python3
"""Small loopback-only TAVIQ editor; source execution is always an explicit Run action."""
import argparse
import json
from pathlib import Path
import subprocess
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from urllib.parse import parse_qs, urlparse

PAGE = """<!doctype html><html><head><meta charset="utf-8"><meta name="viewport" content="width=device-width">
<title>TAVIQ Studio</title><style>
body{font:15px system-ui;background:#10141d;color:#e9eef7;margin:0}header{padding:14px 20px;background:#1c2534;display:flex;gap:14px;align-items:center}
main{display:grid;grid-template-columns:240px 1fr;gap:12px;padding:12px;height:calc(100vh - 80px);box-sizing:border-box}
aside,section{background:#1a2230;border-radius:10px;padding:12px;overflow:auto}button{background:#3978c7;border:0;color:white;padding:9px 14px;border-radius:7px;margin-right:6px}
textarea{width:100%;height:70%;box-sizing:border-box;background:#0c111a;color:#e9eef7;border:1px solid #3b4a62;border-radius:8px;padding:12px;font:14px ui-monospace,monospace}
pre{white-space:pre-wrap;background:#0c111a;padding:12px;border-radius:8px;min-height:80px}li{padding:7px;cursor:pointer}li:hover{background:#2a3548}
@media(max-width:700px){main{grid-template-columns:1fr;height:auto}textarea{height:50vh}}
</style></head><body><header><b>TAVIQ Studio</b><span>Local development environment</span><button id="save">Save</button><button id="run">Run</button></header>
<main><aside><h3>Project files</h3><ul id="files"></ul></aside><section><div id="current">Select a .taviq file</div>
<textarea id="editor" spellcheck="false"></textarea><h3>Output</h3><pre id="output"></pre></section></main>
<script>
let current="";const editor=document.querySelector("#editor"),out=document.querySelector("#output");
async function refresh(){let r=await fetch("/api/files");let d=await r.json();let ul=document.querySelector("#files");ul.textContent="";
for(const f of d.files){let li=document.createElement("li");li.textContent=f;li.onclick=()=>openFile(f);ul.appendChild(li)}}
async function openFile(p){current=p;document.querySelector("#current").textContent=p;let r=await fetch("/api/file?path="+encodeURIComponent(p));let d=await r.json();editor.value=d.content||"";out.textContent=""}
async function save(){if(!current)return;let r=await fetch("/api/file",{method:"PUT",headers:{"Content-Type":"application/json"},body:JSON.stringify({path:current,content:editor.value})});let d=await r.json();out.textContent=d.error||"Saved"}
async function run(){await save();let r=await fetch("/api/run",{method:"POST",headers:{"Content-Type":"application/json"},body:JSON.stringify({path:current})});let d=await r.json();out.textContent=(d.stdout||"")+(d.stderr||"")+(d.error||"")}
document.querySelector("#save").onclick=save;document.querySelector("#run").onclick=run;refresh();
</script></body></html>"""

class Handler(BaseHTTPRequestHandler):
    root = Path(".").resolve()
    compiler = Path("build/taviq").resolve()
    def log_message(self, *_): pass
    def send_json(self, code, data):
        raw = json.dumps(data).encode()
        self.send_response(code); self.send_header("Content-Type","application/json; charset=utf-8")
        self.send_header("Content-Length",str(len(raw))); self.send_header("X-Content-Type-Options","nosniff")
        self.end_headers(); self.wfile.write(raw)
    def allowed_origin(self):
        origin = self.headers.get("Origin")
        if not origin: return True
        parsed = urlparse(origin)
        return parsed.scheme == "http" and parsed.hostname in ("127.0.0.1", "localhost")
    def safe_file(self, name):
        p = Path(name)
        if p.is_absolute() or p.suffix != ".taviq" or ".." in p.parts:
            raise ValueError("only project .taviq files may be accessed")
        target = (self.root / p).resolve()
        if self.root not in target.parents or (target.exists() and not target.is_file()):
            raise ValueError("path is outside the project or not a file")
        return target
    def read_json(self, limit):
        length = int(self.headers.get("Content-Length", "0"))
        if length <= 0 or length > limit:
            raise ValueError("request body is empty or exceeds size limit")
        return json.loads(self.rfile.read(length))
    def do_GET(self):
        u = urlparse(self.path)
        if u.path == "/":
            b=PAGE.encode();self.send_response(200);self.send_header("Content-Type","text/html; charset=utf-8")
            self.send_header("Content-Length",str(len(b)));self.send_header("X-Content-Type-Options","nosniff")
            self.send_header("Content-Security-Policy","default-src 'self'; script-src 'unsafe-inline'; style-src 'unsafe-inline'")
            self.end_headers();self.wfile.write(b);return
        try:
            if u.path == "/api/files":
                files=[str(x.relative_to(self.root)) for x in self.root.rglob("*.taviq")
                       if ".git" not in x.parts and ".taviq" not in x.parts][:200]
                self.send_json(200,{"files":sorted(files)});return
            if u.path == "/api/file":
                name=parse_qs(u.query).get("path",[""])[0];target=self.safe_file(name)
                self.send_json(200,{"path":name,"content":target.read_text(encoding="utf-8")});return
            self.send_json(404,{"error":"not found"})
        except (OSError,ValueError) as exc:self.send_json(400,{"error":str(exc)})
    def do_PUT(self):
        if not self.allowed_origin():return self.send_json(403,{"error":"origin rejected"})
        try:
            data=self.read_json(2_000_000);target=self.safe_file(data["path"])
            if not isinstance(data.get("content"), str):raise ValueError("content must be a string")
            target.parent.mkdir(parents=True,exist_ok=True)
            temp=target.with_suffix(".taviq.part");temp.write_text(data["content"],encoding="utf-8");temp.replace(target)
            self.send_json(200,{"saved":True})
        except (OSError,ValueError,KeyError,TypeError,json.JSONDecodeError) as exc:self.send_json(400,{"error":str(exc)})
    def do_POST(self):
        if not self.allowed_origin():return self.send_json(403,{"error":"origin rejected"})
        try:
            data=self.read_json(8192);target=self.safe_file(data["path"])
            result=subprocess.run([str(self.compiler),"run",str(target)],cwd=self.root,capture_output=True,text=True,timeout=15,check=False)
            self.send_json(200,{"returncode":result.returncode,"stdout":result.stdout,"stderr":result.stderr})
        except (OSError,ValueError,KeyError,TypeError,json.JSONDecodeError,subprocess.TimeoutExpired) as exc:self.send_json(400,{"error":str(exc)})

def main():
    p=argparse.ArgumentParser(description="Run the local TAVIQ Studio editor")
    p.add_argument("--root",default=".");p.add_argument("--compiler",default="build/taviq");p.add_argument("--port",type=int,default=8765)
    a=p.parse_args();Handler.root=Path(a.root).resolve();Handler.compiler=(Handler.root/a.compiler).resolve() if not Path(a.compiler).is_absolute() else Path(a.compiler)
    if not Handler.root.is_dir():p.error("project root does not exist")
    if not Handler.compiler.is_file():p.error("compiler executable not found: "+str(Handler.compiler))
    server=ThreadingHTTPServer(("127.0.0.1",a.port),Handler)
    print("TAVIQ Studio: http://127.0.0.1:%d/ (loopback only; Ctrl+C to stop)"%a.port)
    try:server.serve_forever()
    except KeyboardInterrupt:pass
    finally:server.server_close()
if __name__=="__main__":main()
