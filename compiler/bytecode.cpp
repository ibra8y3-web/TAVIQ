#include "taviq/bytecode.hpp"
#include <bit>
#include <array>
namespace taviq {
namespace {
void putU32(std::ostream& o,std::uint32_t v){for(int i=0;i<4;++i)o.put(static_cast<char>((v>>(i*8))&0xff));}
bool getU32(std::istream& i,std::uint32_t& v){v=0;for(int n=0;n<4;++n){int c=i.get();if(c==EOF)return false;v|=static_cast<std::uint32_t>(static_cast<unsigned char>(c))<<(n*8);}return true;}
bool putString(std::ostream& o,const std::string& s){if(s.size()>1024*1024)return false;putU32(o,static_cast<std::uint32_t>(s.size()));o.write(s.data(),static_cast<std::streamsize>(s.size()));return static_cast<bool>(o);}
bool getString(std::istream& i,std::string& s,std::size_t& total){std::uint32_t n{};if(!getU32(i,n)||n>1024*1024||total+n>16*1024*1024)return false;s.resize(n);if(!i.read(s.data(),static_cast<std::streamsize>(n)))return false;total+=n;return true;}
const char* name(OpCode op){switch(op){case OpCode::PushNumber:return "PUSH_NUMBER";case OpCode::PushString:return "PUSH_STRING";case OpCode::LoadLocal:return "LOAD_LOCAL";case OpCode::StoreLocal:return "STORE_LOCAL";case OpCode::Add:return "ADD";case OpCode::Subtract:return "SUB";case OpCode::Multiply:return "MUL";case OpCode::Divide:return "DIV";case OpCode::Negate:return "NEG";case OpCode::Print:return "PRINT";case OpCode::Halt:return "HALT";}return "INVALID";}
}
bool Bytecode::save(std::ostream& out,std::string& error)const{
 if(code.size()>1000000||locals.size()>65536){error="bytecode exceeds format limits";return false;}
 const std::array<char,8> magic={'T','V','Q','V','M','0','1','\0'};out.write(magic.data(),8);
 putU32(out,static_cast<std::uint32_t>(code.size()));putU32(out,static_cast<std::uint32_t>(locals.size()));
 std::size_t total=0;for(const auto& s:locals){total+=s.size();if(total>16*1024*1024||!putString(out,s)){error="QVM string table exceeds limits";return false;}}
 for(const auto& ins:code){
  out.put(static_cast<char>(ins.op));putU32(out,ins.slot);
  auto bits=std::bit_cast<std::uint64_t>(ins.number);
  for(int n=0;n<8;++n)out.put(static_cast<char>((bits>>(n*8))&0xff));
  total+=ins.text.size();if(total>16*1024*1024||!putString(out,ins.text)){error="QVM operand strings exceed limits";return false;}
 }
 if(!out){error="failed to write bytecode";return false;}return true;
}
bool Bytecode::load(std::istream& in,Bytecode& out,std::string& error){
 std::array<char,8> magic{};if(!in.read(magic.data(),8)||magic!=std::array<char,8>{'T','V','Q','V','M','0','1','\0'}){error="invalid QVM magic or version";return false;}
 std::uint32_t count{},localCount{};
 if(!getU32(in,count)||!getU32(in,localCount)||count>1000000||localCount>65536){error="invalid QVM section sizes";return false;}
 Bytecode tmp;std::size_t total=0;
 for(std::uint32_t n=0;n<localCount;++n){std::string s;if(!getString(in,s,total)){error="invalid QVM local table";return false;}tmp.locals.push_back(std::move(s));}
 for(std::uint32_t n=0;n<count;++n){
  int raw=in.get();std::uint32_t slot{};if(raw<1||raw>11||!getU32(in,slot)){error="invalid QVM instruction";return false;}
  std::uint64_t bits=0;for(int k=0;k<8;++k){int c=in.get();if(c==EOF){error="truncated QVM number";return false;}bits|=static_cast<std::uint64_t>(static_cast<unsigned char>(c))<<(k*8);}
  std::string text;if(!getString(in,text,total)){error="invalid QVM string operand";return false;}
  tmp.code.push_back(Instruction{static_cast<OpCode>(raw),std::bit_cast<double>(bits),slot,std::move(text)});
 }
 out=std::move(tmp);return true;
}
std::string disassemble(const Bytecode& b){
 std::string out;for(std::size_t i=0;i<b.code.size();++i){const auto& x=b.code[i];out+=std::to_string(i)+"  "+name(x.op);
  if(x.op==OpCode::PushNumber)out+=" "+std::to_string(x.number);
  if(x.op==OpCode::PushString)out+=" \""+x.text+"\"";
  if(x.op==OpCode::LoadLocal||x.op==OpCode::StoreLocal)out+=" "+std::to_string(x.slot);
  out+='\n';
 }return out;
}
}