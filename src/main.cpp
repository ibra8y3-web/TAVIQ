#include "taviq/bytecode.hpp"
#include "taviq/compiler.hpp"
#include "taviq/gpu.hpp"
#include "taviq/lexer.hpp"
#include "taviq/native.hpp"
#include "taviq/parser.hpp"
#include "taviq/vm.hpp"
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
namespace {
struct Parsed{taviq::Program program;bool ok{false};};
Parsed parseFile(const std::string& path){
 std::ifstream in(path,std::ios::binary);if(!in){std::cerr<<"TAVIQ: cannot open source file: "<<path<<'\n';return {};}
 std::ostringstream data;data<<in.rdbuf();taviq::Lexer lexer(data.str());auto tokens=lexer.scan();
 if(!lexer.errors().empty()){for(const auto& e:lexer.errors())std::cerr<<e<<'\n';return {};}
 taviq::Parser parser(tokens);auto p=parser.parse();if(!parser.errors().empty()){for(const auto& e:parser.errors())std::cerr<<e<<'\n';return {};}
 return {std::move(p),true};
}
void help(){
 std::cout<<"TAVIQ 0.2.0\nUsage:\n"
 <<"  taviq <file.taviq>             Run source (compatibility form)\n"
 <<"  taviq run <file.taviq>         Run source in QVM\n"
 <<"  taviq check <file.taviq>       Parse and type-check source\n"
 <<"  taviq compile <file> -o <out>  Emit verified QVM bytecode (.tvqbc)\n"
 <<"  taviq run-bytecode <file>      Run QVM bytecode\n"
 <<"  taviq disasm <file>            Disassemble QVM bytecode\n"
 <<"  taviq emit-c <file> -o <out.c> Emit portable C source for a native toolchain\n"
 <<"  taviq gpu-info <provider>      Load a provider and list its compute devices\n"
 <<"  taviq version                  Print version\n";
}
}
int main(int argc,char** argv){
 if(argc==2&&std::string(argv[1])=="version"){std::cout<<"TAVIQ 0.2.0\n";return 0;}
 if(argc==1){help();return 2;}
 std::string command=argv[1];int pathIndex=2;
 if(argc==2){command="run";pathIndex=1;}
 if(command=="help"||command=="--help"||command=="-h"){help();return 0;}
 if(command=="gpu-info"){
  if(argc!=3){help();return 2;}
  taviq::GpuProvider provider;std::string error;
  if(!provider.load(argv[2],error)){std::cerr<<"TAVIQ GPU: "<<error<<'\n';return 1;}
  const int count=provider.deviceCount(error);
  if(count<0){std::cerr<<"TAVIQ GPU: "<<error<<'\n';return 1;}
  std::cout<<"Provider: "<<provider.name()<<"\nDevices: "<<count<<'\n';
  for(int index=0;index<count;++index){
   taviq::GpuDeviceInfo info;
   if(!provider.deviceInfo(index,info,error)){std::cerr<<"TAVIQ GPU: "<<error<<'\n';return 1;}
   std::cout<<index<<": "<<info.name<<" ["<<info.backend<<"] memory="<<info.memoryBytes<<" bytes\n";
  }
  return 0;
 }
 if(command=="run-bytecode"||command=="disasm"){
  if(argc!=3){help();return 2;}std::ifstream in(argv[2],std::ios::binary);if(!in){std::cerr<<"cannot open bytecode file\n";return 2;}
  taviq::Bytecode b;std::string error;if(!taviq::Bytecode::load(in,b,error)){std::cerr<<error<<'\n';return 1;}
  if(command=="disasm"){std::cout<<taviq::disassemble(b);return 0;}
  return taviq::VM{}.run(b,std::cout,error)?(std::cerr<<error<<'\n',1):0;
 }
 if(command!="run"&&command!="check"&&command!="compile"&&command!="emit-c"){std::cerr<<"unknown command: "<<command<<'\n';help();return 2;}
 if(argc<=pathIndex){help();return 2;}const std::string input=argv[pathIndex];std::string output;
 if(command=="compile"||command=="emit-c"){
  if(argc!=5||std::string(argv[3])!="-o"){help();return 2;}output=argv[4];
 }else if(argc!=pathIndex+1){help();return 2;}
 auto parsed=parseFile(input);if(!parsed.ok)return 1;
 taviq::Bytecode bytecode;std::vector<std::string> errors;
 if(command=="emit-c"){
  std::string c;if(!taviq::emitC(parsed.program,c,errors)){for(const auto& e:errors)std::cerr<<e<<'\n';return 1;}
  std::ofstream out(output,std::ios::binary);out<<c;if(!out){std::cerr<<"failed to write C output\n";return 1;}return 0;
 }
 if(!taviq::compile(parsed.program,bytecode,errors)){for(const auto& e:errors)std::cerr<<e<<'\n';return 1;}
 if(command=="check"){std::cout<<"TAVIQ: source is valid\n";return 0;}
 if(command=="compile"){
  std::ofstream out(output,std::ios::binary);std::string error;if(!out||!bytecode.save(out,error)){std::cerr<<(error.empty()?"cannot open output file":error)<<'\n';return 1;}return 0;
 }
 std::string error;return taviq::VM{}.run(bytecode,std::cout,error)?(std::cerr<<error<<'\n',1):0;
}
