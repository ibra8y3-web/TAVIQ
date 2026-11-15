#!/usr/bin/env python3
"""Generate deterministic CycloneDX 1.6 inventory for TAVIQ source and build inputs."""
import argparse
from datetime import datetime, timezone
import hashlib
import json
import os
from pathlib import Path
import uuid

SKIP_DIRS={".git","build","build-android","build-codeql","dist",".gradle","__pycache__",".idea"}

def generate(root,version,epoch=None):
    root=Path(root).resolve()
    epoch=int(os.environ.get("SOURCE_DATE_EPOCH","0")) if epoch is None else int(epoch)
    if epoch<0:
        raise ValueError("SOURCE_DATE_EPOCH must not be negative")
    timestamp=datetime.fromtimestamp(epoch,timezone.utc).isoformat(timespec="seconds").replace("+00:00","Z")
    components=[]
    for path in sorted(root.rglob("*")):
        relative=path.relative_to(root)
        if any(part in SKIP_DIRS for part in relative.parts):
            continue
        if not path.is_file() or path.is_symlink():
            continue
        digest=hashlib.sha256(path.read_bytes()).hexdigest()
        components.append({"type":"file","name":relative.as_posix(),"version":version,
                           "hashes":[{"alg":"SHA-256","content":digest}]})
    seed="\n".join(item["name"]+item["hashes"][0]["content"] for item in components)
    stable_uuid=uuid.UUID(hashlib.sha256((version+"\n"+seed).encode()).hexdigest()[:32])
    return {
        "bomFormat":"CycloneDX","specVersion":"1.6",
        "serialNumber":"urn:uuid:"+str(stable_uuid),"version":1,
        "metadata":{"timestamp":timestamp,"component":{"type":"application","name":"TAVIQ","version":version}},
        "components":components,
        "properties":[{"name":"taviq:sbom-scope","value":"repository source inventory; operating-system libraries and external SDKs are not included"}]
    }

def main():
    parser=argparse.ArgumentParser(description="Generate deterministic CycloneDX SBOM")
    parser.add_argument("--root",default=".")
    parser.add_argument("--version",required=True)
    parser.add_argument("--output",default="dist/taviq.cdx.json")
    args=parser.parse_args()
    try:
        data=generate(args.root,args.version)
        output=Path(args.output);output.parent.mkdir(parents=True,exist_ok=True)
        temporary=output.with_name(output.name+".part")
        temporary.write_text(json.dumps(data,ensure_ascii=False,sort_keys=True,indent=2)+"\n",encoding="utf-8")
        temporary.replace(output)
        print("CycloneDX 1.6:",output,"components:",len(data["components"]))
        return 0
    except (OSError,ValueError,OverflowError) as exc:
        print("taviq-sbom:",exc)
        return 1

if __name__=="__main__":
    raise SystemExit(main())
