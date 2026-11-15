from pathlib import Path
import subprocess
import sys
import tempfile
def run(args,cwd,expect=0):
 r=subprocess.run(args,cwd=cwd,text=True,capture_output=True,timeout=60)
 if r.returncode!=expect:raise AssertionError("command %r returned %s\nstdout=%s\nstderr=%s"%(args,r.returncode,r.stdout,r.stderr))
 return r
def main():
 binary=Path(sys.argv[1]).resolve();cc=sys.argv[2] if len(sys.argv)>2 else ""
 with tempfile.TemporaryDirectory(prefix="taviq-cli-") as d:
  root=Path(d);source=root/"smoke.taviq";source.write_text('flow main { let x = sqrt(36) + abs(-6); display x; display len("abc"); }\n',encoding="utf-8")
  assert "12" in run([str(binary),"run",str(source)],root).stdout
  assert "valid" in run([str(binary),"check",str(source)],root).stdout
  bc=root/"smoke.tvqbc";run([str(binary),"compile",str(source),"-o",str(bc)],root)
  assert "12" in run([str(binary),"run-bytecode",str(bc)],root).stdout
  assert "LENGTH" in run([str(binary),"disasm",str(bc)],root).stdout
  gen=root/"smoke.c";run([str(binary),"emit-c",str(source),"-o",str(gen)],root)
  src=root/"native-src";build=root/"native-build";src.mkdir();(src/"main.c").write_text(gen.read_text(encoding="utf-8"),encoding="utf-8")
  cmake=["cmake_minimum_required(VERSION 3.20)","project(TAVIQNativeSmoke LANGUAGES C)","add_executable(taviq_native_smoke main.c)","if(NOT MSVC)","target_link_libraries(taviq_native_smoke PRIVATE m)","endif()",""]
  (src/"CMakeLists.txt").write_text("\n".join(cmake),encoding="utf-8")
  args=["cmake","-S",str(src),"-B",str(build),"-DCMAKE_BUILD_TYPE=Release"]
  if cc:args.append("-DCMAKE_C_COMPILER="+cc)
  run(args,root);run(["cmake","--build",str(build),"--config","Release","--parallel"],root)
  options=[build/"Release"/"taviq_native_smoke.exe",build/"taviq_native_smoke.exe",build/"Release"/"taviq_native_smoke",build/"taviq_native_smoke"]
  exe=next((p for p in options if p.is_file()),None)
  if exe is None:raise AssertionError("native C executable not found")
  result=run([str(exe)],root);assert "12" in result.stdout and "3" in result.stdout
 return 0
if __name__=="__main__":raise SystemExit(main())
