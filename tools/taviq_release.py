#!/usr/bin/env python3
"""Build deterministic TAVIQ archives and checksum manifests."""
import argparse
from datetime import datetime
import gzip
import hashlib
import json
import os
from pathlib import Path
import tarfile
from io import BytesIO
import zipfile

def digest(data):
    return hashlib.sha256(data).hexdigest()

def build(binary,sbom,output,version,platform,extras=(),epoch=None):
    binary=Path(binary).resolve();sbom=Path(sbom).resolve()
    if not binary.is_file() or not sbom.is_file():
        raise ValueError("binary and SBOM files must exist")
    if not version or not platform or any(c in version+platform for c in "/\\"):
        raise ValueError("invalid release version or platform")
    epoch=int(os.environ.get("SOURCE_DATE_EPOCH","0")) if epoch is None else int(epoch)
    if epoch<0:
        raise ValueError("release timestamp must not be negative")
    files={"bin/"+binary.name:binary.read_bytes(),"sbom.cdx.json":sbom.read_bytes()}
    for extra in extras:
        path=Path(extra).resolve()
        if path.is_file():
            files[path.name]=path.read_bytes()
    manifest={"name":"TAVIQ","version":version,"platform":platform,"source_date_epoch":epoch,
              "files":[{"path":name,"sha256":digest(data),"size":len(data)}
                       for name,data in sorted(files.items())]}
    files["manifest.json"]=(json.dumps(manifest,sort_keys=True,indent=2)+"\n").encode()
    output=Path(output);output.mkdir(parents=True,exist_ok=True)
    stem="taviq-"+version+"-"+platform
    buffer=BytesIO()
    with gzip.GzipFile(filename="",mode="wb",fileobj=buffer,mtime=epoch,compresslevel=9) as compressed:
        with tarfile.open(fileobj=compressed,mode="w",format=tarfile.USTAR_FORMAT) as archive:
            for name,data in sorted(files.items()):
                info=tarfile.TarInfo(name);info.size=len(data);info.mtime=epoch
                info.uid=0;info.gid=0;info.uname="";info.gname=""
                info.mode=0o755 if name.startswith("bin/") else 0o644
                archive.addfile(info,BytesIO(data))
    tar_data=buffer.getvalue()
    tar_path=output/(stem+".tar.gz");tar_path.write_bytes(tar_data)
    zip_path=output/(stem+".zip")
    with zipfile.ZipFile(zip_path,"w",compression=zipfile.ZIP_STORED) as archive:
        for name,data in sorted(files.items()):
            info=zipfile.ZipInfo(name,date_time=(1980,1,1,0,0,0))
            info.compress_type=zipfile.ZIP_STORED;info.create_system=3
            info.external_attr=(0o100755 if name.startswith("bin/") else 0o100644)<<16
            archive.writestr(info,data)
    sums=(digest(tar_data)+"  "+tar_path.name+"\n"+
          digest(zip_path.read_bytes())+"  "+zip_path.name+"\n")
    sums_path=output/(stem+".SHA256SUMS");sums_path.write_text(sums,encoding="ascii")
    return tar_path,zip_path,sums_path

def main():
    parser=argparse.ArgumentParser(description="Build reproducible TAVIQ release archives")
    parser.add_argument("--binary",required=True);parser.add_argument("--sbom",required=True)
    parser.add_argument("--output",default="dist");parser.add_argument("--version",required=True)
    parser.add_argument("--platform",required=True);parser.add_argument("--extra",action="append",default=[])
    args=parser.parse_args()
    try:
        for path in build(args.binary,args.sbom,args.output,args.version,args.platform,args.extra):
            print(path)
        return 0
    except (OSError,ValueError,OverflowError) as exc:
        print("taviq-release:",exc)
        return 1

if __name__=="__main__":
    raise SystemExit(main())
