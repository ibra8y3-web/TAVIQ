#include "taviq/ai.hpp"
#include "taviq/bytecode.hpp"
#include "taviq/compiler.hpp"
#include "taviq/gpu.hpp"
#include "taviq/lexer.hpp"
#include "taviq/native.hpp"
#include "taviq/parser.hpp"
#include "taviq/vm.hpp"
#include <algorithm>
#include <cmath>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>
namespace {
struct Parsed{taviq::Program program;bool ok{false};};
Parsed parseFile(const std::string& path){
 std::ifstream in(path,std::ios::binary);if(!in){std::cerr<<"TAVIQ: cannot open source file: "<<path<<'\n';return {};}
 std::ostringstream data;data<<in.rdbuf();taviq::Lexer lexer(data.str());auto tokens=lexer.scan();
 if(!lexer.errors().empty()){for(const auto& e:lexer.errors())std::cerr<<e<<'\n';return {};}
 taviq::Parser parser(tokens);auto p=parser.parse();if(!parser.errors().empty()){for(const auto& e:parser.errors())std::cerr<<e<<'\n';return {};}
 return {std::move(p),true};
}
bool parseFloatList(const std::string& value,std::vector<float>& output){
 output.clear();if(value.empty())return false;
 std::stringstream stream(value);std::string part;
 while(std::getline(stream,part,',')){
  if(part.empty())return false;
  std::size_t consumed=0;
  try{
   const float number=std::stof(part,&consumed);
   if(consumed!=part.size()||!std::isfinite(number))return false;
   output.push_back(number);
  }catch(const std::exception&){return false;}
  if(output.size()>16*1024*1024)return false;
 }
 return !output.empty()&&value.back()!=',';
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
 <<"  taviq gpu-add <provider> <device> <a,b,...> <c,d,...>  Execute float32 vector addition\n"
 <<"  taviq ai-generate <provider> <prompt> [tokens] [temperature]\n"
 <<"  taviq debug <file.taviq> [comma-separated-breakpoints]\n"
 <<"  taviq version                  Print version\n";
}
}
int main(int argc,char** argv){
 if(argc==2&&std::string(argv[1])=="version"){std::cout<<"TAVIQ 0.2.0\n";return 0;}
 if(argc==1){help();return 2;}
 std::string command=argv[1];int pathIndex=2;
 if(argc==2){command="run";pathIndex=1;}
 if(command=="help"||command=="--help"||command=="-h"){help();return 0;}
 if(command=="ai-generate"){
  if(argc<4||argc>6){help();return 2;}
  std::uint32_t tokens=128;double temperature=0.7;
  try{
   if(argc>=5)tokens=static_cast<std::uint32_t>(std::stoul(argv[4]));
   if(argc>=6)temperature=std::stod(argv[5]);
  }catch(const std::exception&){std::cerr<<"invalid AI generation parameter\n";return 2;}
  taviq::AiProvider provider;std::string error;taviq::AiResult result;
  if(!provider.load(argv[2],"{}",error)){std::cerr<<"TAVIQ AI: "<<error<<'\n';return 1;}
  if(!provider.generate(argv[3],tokens,temperature,result,error)){std::cerr<<"TAVIQ AI: "<<error<<'\n';return 1;}
  std::cout<<result.text;return 0;
 }
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
 if(command=="gpu-add"){
  if(argc!=6){help();return 2;}
  long deviceIndex=-1;
  try{
   std::size_t consumed=0;deviceIndex=std::stol(argv[3],&consumed);
   if(consumed!=std::string(argv[3]).size()||deviceIndex<0||deviceIndex>255)
    throw std::out_of_range("device");
  }catch(const std::exception&){std::cerr<<"TAVIQ GPU: invalid device index\n";return 2;}
  std::vector<float> left,right,result;
  if(!parseFloatList(argv[4],left)||!parseFloatList(argv[5],right)){
   std::cerr<<"TAVIQ GPU: vectors must be comma-separated finite float values\n";return 2;
  }
  if(left.size()!=right.size()){
   std::cerr<<"TAVIQ GPU: vectors must have equal lengths\n";return 2;
  }
  taviq::GpuProvider provider;std::string error;
  if(!provider.load(argv[2],error)){std::cerr<<"TAVIQ GPU: "<<error<<'\n';return 1;}
  if(!provider.vectorAddF32(static_cast<int>(deviceIndex),left,right,result,error)){
   std::cerr<<"TAVIQ GPU: "<<error<<'\n';return 1;
  }
  for(std::size_t i=0;i<result.size();++i){if(i)std::cout<<',';std::cout<<result[i];}
  std::cout<<'\n';return 0;
 }
 if(command=="run-bytecode"||command=="disasm"){
  if(argc!=3){help();return 2;}std::ifstream in(argv[2],std::ios::binary);if(!in){std::cerr<<"cannot open bytecode file\n";return 2;}
  taviq::Bytecode b;std::string error;if(!taviq::Bytecode::load(in,b,error)){std::cerr<<error<<'\n';return 1;}
  if(command=="disasm"){std::cout<<taviq::disassemble(b);return 0;}
  return taviq::VM{}.run(b,std::cout,error)?(std::cerr<<error<<'\n',1):0;
 }
 if(command!="run"&&command!="debug"&&command!="check"&&command!="compile"&&command!="emit-c"){
  std::cerr<<"unknown command: "<<command<<'\n';help();return 2;
 }
 if(argc<=pathIndex){help();return 2;}const std::string input=argv[pathIndex];std::string output;
 if(command=="compile"||command=="emit-c"){
  if(argc!=5||std::string(argv[3])!="-o"){help();return 2;}output=argv[4];
 }else if(command=="debug"){if(argc>4){help();return 2;}}
 else if(argc!=pathIndex+1){help();return 2;}
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
 std::string error;
 if(command=="debug"){
  std::vector<std::size_t> breakpoints;
  if(argc==4){
   std::string values=argv[3];std::stringstream parts(values);std::string item;
   while(std::getline(parts,item,',')){
    try{const auto value=std::stoull(item);if(value>=bytecode.code.size()){std::cerr<<"breakpoint outside bytecode\n";return 2;}breakpoints.push_back(static_cast<std::size_t>(value));}
    catch(const std::exception&){std::cerr<<"invalid breakpoint list\n";return 2;}
   }
  }
  const auto trace=[&](std::size_t pc,const taviq::Instruction& instruction){
   if(breakpoints.empty()||std::find(breakpoints.begin(),breakpoints.end(),pc)!=breakpoints.end())
    std::cerr<<(breakpoints.empty()?"TRACE ":"BREAK ")<<pc<<" opcode="<<static_cast<unsigned>(instruction.op)<<"\n";
  };
  const int status=taviq::VM{}.run(bytecode,std::cout,error,1000000,trace);
  if(status){std::cerr<<error<<'\n';return 1;}
  return 0;
 }
 return taviq::VM{}.run(bytecode,std::cout,error)?(std::cerr<<error<<'\n',1):0;
}
