#include "taviq/bytecode.hpp"
#include <array>
#include <bit>
#include <cmath>
#include <cstdio>
#include <istream>
#include <ostream>
#include <unordered_set>
#include <utility>
namespace taviq {
namespace {
void putU32(std::ostream& o,std::uint32_t v){for(int i=0;i<4;++i)o.put(static_cast<char>((v>>(i*8))&0xff));}
bool getU32(std::istream& i,std::uint32_t& v){v=0;for(int n=0;n<4;++n){int c=i.get();if(c==EOF)return false;v|=static_cast<std::uint32_t>(static_cast<unsigned char>(c))<<(n*8);}return true;}
bool putString(std::ostream& o,const std::string& s){if(s.size()>1024*1024)return false;putU32(o,static_cast<std::uint32_t>(s.size()));o.write(s.data(),static_cast<std::streamsize>(s.size()));return static_cast<bool>(o);}
bool getString(std::istream& i,std::string& s,std::size_t& total){std::uint32_t n{};if(!getU32(i,n)||n>1024*1024||total+n>16*1024*1024)return false;s.resize(n);if(!i.read(s.data(),static_cast<std::streamsize>(n)))return false;total+=n;return true;}
const char* name(OpCode op){switch(op){case OpCode::PushNumber:return "PUSH_NUMBER";case OpCode::PushString:return "PUSH_STRING";case OpCode::LoadLocal:return "LOAD_LOCAL";case OpCode::StoreLocal:return "STORE_LOCAL";case OpCode::Add:return "ADD";case OpCode::Subtract:return "SUB";case OpCode::Multiply:return "MUL";case OpCode::Divide:return "DIV";case OpCode::Negate:return "NEG";case OpCode::Print:return "PRINT";case OpCode::Halt:return "HALT";}return "INVALID";}
}
bool verifyBytecode(const Bytecode& b,std::string& error){
 error.clear();
 auto fail=[&](const std::string& message){error=message;return false;};
 if(b.code.empty()||b.code.size()>1000000)return fail("QVM instruction count is empty or exceeds the v1 limit");
 if(b.locals.size()>65536)return fail("QVM local table exceeds the v1 limit");
 std::unordered_set<std::string> names;std::size_t total=0;
 for(const auto& local:b.locals){
  if(local.empty()||local.size()>1024*1024||!names.insert(local).second)return fail("QVM local names must be non-empty, bounded and unique");
  total+=local.size();if(total>16*1024*1024)return fail("QVM string table exceeds the v1 limit");
 }
 enum class ValueType : std::uint8_t { Number, String };
 std::vector<int> localTypes(b.locals.size(),-1);
 std::vector<ValueType> stack;
 bool halted=false;
 auto pop=[&](ValueType& value){if(stack.empty())return false;value=stack.back();stack.pop_back();return true;};
 for(std::size_t pc=0;pc<b.code.size();++pc){
  const auto& ins=b.code[pc];
  total+=ins.text.size();
  if(ins.text.size()>1024*1024||total>16*1024*1024)return fail("QVM string operands exceed the v1 limit");
  switch(ins.op){
   case OpCode::PushNumber:
    if(!std::isfinite(ins.number))return fail("QVM contains a non-finite numeric constant");
    stack.push_back(ValueType::Number);break;
   case OpCode::PushString:
    stack.push_back(ValueType::String);break;
   case OpCode::LoadLocal:
    if(ins.slot>=localTypes.size()||localTypes[ins.slot]<0)return fail("QVM loads an invalid or uninitialized local slot");
    stack.push_back(static_cast<ValueType>(localTypes[ins.slot]));break;
   case OpCode::StoreLocal:{
    ValueType value;if(ins.slot>=localTypes.size()||!pop(value))return fail("QVM stores to an invalid local slot or underflows the stack");
    if(localTypes[ins.slot]>=0&&localTypes[ins.slot]!=static_cast<int>(value))return fail("QVM local value type changes after initialization");
    localTypes[ins.slot]=static_cast<int>(value);break;
   }
   case OpCode::Add:case OpCode::Subtract:case OpCode::Multiply:case OpCode::Divide:{
    ValueType rhs,lhs;if(!pop(rhs)||!pop(lhs))return fail("QVM arithmetic instruction underflows the stack");
    if(ins.op==OpCode::Add&&lhs==ValueType::String&&rhs==ValueType::String){stack.push_back(ValueType::String);break;}
    if(lhs!=ValueType::Number||rhs!=ValueType::Number)return fail("QVM arithmetic operands have incompatible types");
    stack.push_back(ValueType::Number);break;
   }
   case OpCode::Negate:{
    ValueType value;if(!pop(value)||value!=ValueType::Number)return fail("QVM NEG requires one numeric stack value");
    stack.push_back(ValueType::Number);break;
   }
   case OpCode::Print:{
    ValueType value;if(!pop(value))return fail("QVM PRINT underflows the stack");
    break;
   }
   case OpCode::Halt:
    if(pc+1!=b.code.size())return fail("QVM HALT must be the final instruction");
    if(!stack.empty())return fail("QVM HALT requires an empty operand stack");
    halted=true;break;
   default:return fail("QVM contains an unknown opcode");
  }
  if(stack.size()>65536)return fail("QVM operand stack exceeds the v1 limit");
 }
 if(!halted)return fail("QVM bytecode has no final HALT instruction");
 return true;
}
bool Bytecode::save(std::ostream& out,std::string& error)const{
 if(!verifyBytecode(*this,error))return false;
 const std::array<char,8> magic={'T','V','Q','V','M','0','1','\0'};out.write(magic.data(),8);
 putU32(out,static_cast<std::uint32_t>(code.size()));putU32(out,static_cast<std::uint32_t>(locals.size()));
 for(const auto& s:locals)if(!putString(out,s)){error="failed to write QVM local table";return false;}
 for(const auto& ins:code){
  out.put(static_cast<char>(ins.op));putU32(out,ins.slot);
  auto bits=std::bit_cast<std::uint64_t>(ins.number);
  for(int n=0;n<8;++n)out.put(static_cast<char>((bits>>(n*8))&0xff));
  if(!putString(out,ins.text)){error="failed to write QVM operand";return false;}
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
 if(!verifyBytecode(tmp,error))return false;
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
