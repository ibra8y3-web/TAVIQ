#include "taviq/bytecode.hpp"
#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstdio>
#include <deque>
#include <istream>
#include <optional>
#include <ostream>
#include <unordered_set>
#include <utility>
namespace taviq {
namespace {
constexpr std::size_t MaxInstructions=1'000'000,MaxLocals=65'536;
constexpr std::size_t MaxString=1*1024*1024,MaxEncoded=16*1024*1024,MaxLive=16*1024*1024;
enum class ValueType:std::uint8_t{Number,String,Boolean};
struct StackValue{ValueType type{};std::size_t stringBytes{};};
struct LocalState{
 bool mayInit{false},definite{false},mutableLocal{false};
 ValueType type{ValueType::Number};std::size_t stringBytes{};
};
struct State{std::vector<StackValue> stack;std::vector<LocalState> locals;};
void putU32(std::ostream& o,std::uint32_t v){for(int i=0;i<4;++i)o.put(static_cast<char>((v>>(i*8))&0xff));}
bool getU32(std::istream& i,std::uint32_t& v){
 v=0;for(int n=0;n<4;++n){int c=i.get();if(c==EOF)return false;v|=static_cast<std::uint32_t>(static_cast<unsigned char>(c))<<(n*8);}return true;
}
bool putString(std::ostream& o,const std::string& s){
 if(s.size()>MaxString)return false;putU32(o,static_cast<std::uint32_t>(s.size()));
 o.write(s.data(),static_cast<std::streamsize>(s.size()));return static_cast<bool>(o);
}
bool getString(std::istream& i,std::string& s,std::size_t& total){
 std::uint32_t n{};if(!getU32(i,n)||n>MaxString||n>MaxEncoded-total)return false;
 s.resize(n);if(!i.read(s.data(),static_cast<std::streamsize>(n)))return false;total+=n;return true;
}
const char* name(OpCode op){
 switch(op){
 case OpCode::PushNumber:return "PUSH_NUMBER";case OpCode::PushString:return "PUSH_STRING";
 case OpCode::LoadLocal:return "LOAD_LOCAL";case OpCode::StoreLocal:return "STORE_LOCAL";
 case OpCode::Add:return "ADD";case OpCode::Subtract:return "SUB";case OpCode::Multiply:return "MUL";
 case OpCode::Divide:return "DIV";case OpCode::Negate:return "NEG";case OpCode::Print:return "PRINT";
 case OpCode::Halt:return "HALT";case OpCode::Length:return "LENGTH";case OpCode::SquareRoot:return "SQRT";
 case OpCode::Absolute:return "ABS";case OpCode::PushBoolean:return "PUSH_BOOLEAN";
 case OpCode::Equal:return "EQUAL";case OpCode::NotEqual:return "NOT_EQUAL";case OpCode::Less:return "LESS";
 case OpCode::LessEqual:return "LESS_EQUAL";case OpCode::Greater:return "GREATER";
 case OpCode::GreaterEqual:return "GREATER_EQUAL";case OpCode::LogicalNot:return "LOGICAL_NOT";
 case OpCode::Jump:return "JUMP";case OpCode::JumpIfFalse:return "JUMP_IF_FALSE";
 case OpCode::StoreLocalMutable:return "STORE_LOCAL_MUTABLE";
 }return "INVALID";
}
bool validOpcode(std::uint8_t op,std::uint32_t version){return op>=1&&(version==1?op<=14:(version==2&&op<=25));}
std::size_t liveBytes(const State& s){
 std::size_t total=0;
 for(const auto& x:s.locals)if(x.mayInit){if(x.stringBytes>MaxLive-total)return MaxLive+1;total+=x.stringBytes;}
 for(const auto& x:s.stack){if(x.stringBytes>MaxLive-total)return MaxLive+1;total+=x.stringBytes;}
 return total;
}
bool sameState(const State& a,const State& b){
 if(a.stack.size()!=b.stack.size()||a.locals.size()!=b.locals.size())return false;
 for(std::size_t i=0;i<a.stack.size();++i)
  if(a.stack[i].type!=b.stack[i].type||a.stack[i].stringBytes!=b.stack[i].stringBytes)return false;
 for(std::size_t i=0;i<a.locals.size();++i){
  const auto& x=a.locals[i];const auto& y=b.locals[i];
  if(x.mayInit!=y.mayInit||x.definite!=y.definite||x.mutableLocal!=y.mutableLocal||
     x.type!=y.type||x.stringBytes!=y.stringBytes)return false;
 }
 return true;
}
}
bool verifyBytecode(const Bytecode& b,std::string& error){
 error.clear();auto fail=[&](const std::string& m){error=m;return false;};
 if(b.formatVersion!=1&&b.formatVersion!=2)return fail("unsupported QVM bytecode version");
 if(b.code.empty()||b.code.size()>MaxInstructions)return fail("QVM instruction count is empty or exceeds limit");
 if(b.locals.size()>MaxLocals)return fail("QVM local table exceeds limit");
 std::unordered_set<std::string> names;std::size_t encoded=0;
 for(const auto& s:b.locals){
  if(s.empty()||s.size()>MaxString||!names.insert(s).second)return fail("QVM local names must be non-empty, bounded and unique");
  if(s.size()>MaxEncoded-encoded)return fail("QVM local-name table exceeds encoded string limit");encoded+=s.size();
 }
 for(std::size_t pc=0;pc<b.code.size();++pc){
  const auto& x=b.code[pc];const auto raw=static_cast<std::uint8_t>(x.op);
  if(!validOpcode(raw,b.formatVersion))return fail("QVM contains an unknown or version-incompatible opcode");
  if(x.text.size()>MaxString||x.text.size()>MaxEncoded-encoded)return fail("QVM string operands exceed encoded string limit");
  encoded+=x.text.size();
  if(x.op==OpCode::PushNumber&&!std::isfinite(x.number))return fail("QVM contains non-finite numeric constant");
  if(x.op==OpCode::PushBoolean&&x.slot>1)return fail("QVM boolean constant must be zero or one");
  if(x.op==OpCode::Jump||x.op==OpCode::JumpIfFalse)
   if(b.formatVersion<2||x.slot>=b.code.size())return fail("QVM jump target is outside instruction stream");
  if(x.op==OpCode::Halt&&pc+1!=b.code.size())return fail("QVM HALT must be final instruction");
 }
 if(b.code.back().op!=OpCode::Halt)return fail("QVM bytecode must end with HALT");

 std::vector<std::optional<State>> incoming(b.code.size());
 State initial;initial.locals.resize(b.locals.size());incoming[0]=initial;
 std::deque<std::size_t> work{0};bool reachedHalt=false;
 const auto enqueue=[&](std::size_t target,const State& candidate,std::size_t from)->bool{
  if(target>=b.code.size()){error="QVM control flow falls outside instruction stream";return false;}
  if(liveBytes(candidate)>MaxLive){error="QVM live string memory exceeds 16 MiB";return false;}
  if(!incoming[target]){incoming[target]=candidate;work.push_back(target);return true;}
  State merged=*incoming[target];
  if(merged.stack.size()!=candidate.stack.size()){
   error="QVM control-flow paths reach an instruction with different stack heights";return false;
  }
  for(std::size_t i=0;i<merged.stack.size();++i){
   auto& a=merged.stack[i];const auto& c=candidate.stack[i];
   if(a.type!=c.type){error="QVM control-flow paths merge incompatible stack types";return false;}
   if(a.stringBytes!=c.stringBytes){
    if(from>=target){error="QVM loop carries an unbounded or varying string value";return false;}
    a.stringBytes=std::max(a.stringBytes,c.stringBytes);
   }
  }
  for(std::size_t i=0;i<merged.locals.size();++i){
   auto& a=merged.locals[i];const auto& c=candidate.locals[i];
   if(a.mayInit&&c.mayInit){
    if(a.type!=c.type){error="QVM paths initialize a local with different types";return false;}
    if(a.mutableLocal!=c.mutableLocal){error="QVM paths disagree about local mutability";return false;}
    if(a.stringBytes!=c.stringBytes){
     if(from>=target){error="QVM loop carries a string local with unbounded or varying size";return false;}
     a.stringBytes=std::max(a.stringBytes,c.stringBytes);
    }
    a.definite=a.definite&&c.definite;
   }else if(a.mayInit||c.mayInit){
    const auto initialized=a.mayInit?a:c;
    a.mayInit=true;a.definite=false;a.mutableLocal=initialized.mutableLocal;
    a.type=initialized.type;a.stringBytes=initialized.stringBytes;
   }else a=LocalState{};
  }
  if(liveBytes(merged)>MaxLive){error="QVM live string memory exceeds 16 MiB at control-flow merge";return false;}
  if(!sameState(*incoming[target],merged)){incoming[target]=std::move(merged);work.push_back(target);}
  return true;
 };

 while(!work.empty()){
  const auto pc=work.front();work.pop_front();State s=*incoming[pc];const auto& x=b.code[pc];
  const auto pop=[&](StackValue& v){if(s.stack.empty())return false;v=s.stack.back();s.stack.pop_back();return true;};
  const auto push=[&](ValueType t,std::size_t n=0){s.stack.push_back({t,n});return liveBytes(s)<=MaxLive;};
  bool falls=true;
  switch(x.op){
  case OpCode::PushNumber:if(!push(ValueType::Number))return fail("QVM live string memory exceeds 16 MiB");break;
  case OpCode::PushString:if(!push(ValueType::String,x.text.size()))return fail("QVM live string memory exceeds 16 MiB");break;
  case OpCode::PushBoolean:if(!push(ValueType::Boolean))return fail("QVM live string memory exceeds 16 MiB");break;
  case OpCode::LoadLocal:{
   if(x.slot>=s.locals.size()||!s.locals[x.slot].definite)return fail("QVM loads invalid, uninitialized or path-dependent local");
   const auto& l=s.locals[x.slot];if(!push(l.type,l.stringBytes))return fail("QVM live string memory exceeds 16 MiB");break;
  }
  case OpCode::StoreLocal:case OpCode::StoreLocalMutable:{
   if(x.slot>=s.locals.size())return fail("QVM store references invalid local slot");
   StackValue value{};if(!pop(value))return fail("QVM local store underflows stack");
   auto& l=s.locals[x.slot];
   if(x.op==OpCode::StoreLocal){
    if(l.mayInit)return fail("QVM immutable local is initialized more than once");
    l={true,true,false,value.type,value.stringBytes};
   }else{
    if(l.mayInit&&!l.mutableLocal)return fail("QVM mutable store attempts to overwrite immutable local");
    if(l.mayInit&&l.type!=value.type)return fail("QVM mutable local assignment changes type");
    l={true,true,true,value.type,value.stringBytes};
   }
   if(liveBytes(s)>MaxLive)return fail("QVM live string memory exceeds 16 MiB");break;
  }
  case OpCode::Add:case OpCode::Subtract:case OpCode::Multiply:case OpCode::Divide:{
   if(s.stack.size()<2)return fail("QVM arithmetic instruction underflows stack");
   const auto rhs=s.stack.back(),lhs=s.stack[s.stack.size()-2];
   if(x.op==OpCode::Add&&lhs.type==ValueType::String&&rhs.type==ValueType::String){
    const auto before=liveBytes(s);
    if(lhs.stringBytes>MaxLive-rhs.stringBytes)return fail("QVM string concatenation exceeds 16 MiB");
    const auto result=lhs.stringBytes+rhs.stringBytes;
    if(result>MaxLive-before)return fail("QVM string concatenation peak memory exceeds 16 MiB");
    StackValue tmp{};pop(tmp);pop(tmp);
    if(!push(ValueType::String,result))return fail("QVM live string memory exceeds 16 MiB");break;
   }
   if(lhs.type!=ValueType::Number||rhs.type!=ValueType::Number)
    return fail("QVM arithmetic operands must be numbers (except string + string)");
   StackValue tmp{};pop(tmp);pop(tmp);if(!push(ValueType::Number))return fail("QVM live string memory exceeds 16 MiB");break;
  }
  case OpCode::Negate:case OpCode::SquareRoot:case OpCode::Absolute:{
   StackValue v{};if(!pop(v)||v.type!=ValueType::Number)return fail("QVM numeric unary operation requires number");
   if(!push(ValueType::Number))return fail("QVM live string memory exceeds 16 MiB");break;
  }
  case OpCode::Length:{
   StackValue v{};if(!pop(v)||v.type!=ValueType::String)return fail("QVM LENGTH requires string");
   if(!push(ValueType::Number))return fail("QVM live string memory exceeds 16 MiB");break;
  }
  case OpCode::LogicalNot:{
   StackValue v{};if(!pop(v)||v.type!=ValueType::Boolean)return fail("QVM NOT requires boolean");
   if(!push(ValueType::Boolean))return fail("QVM live string memory exceeds 16 MiB");break;
  }
  case OpCode::Equal:case OpCode::NotEqual:{
   if(s.stack.size()<2)return fail("QVM equality instruction underflows stack");
   const auto rhs=s.stack.back(),lhs=s.stack[s.stack.size()-2];
   if(lhs.type!=rhs.type)return fail("QVM equality operands must have same type");
   StackValue tmp{};pop(tmp);pop(tmp);if(!push(ValueType::Boolean))return fail("QVM live string memory exceeds 16 MiB");break;
  }
  case OpCode::Less:case OpCode::LessEqual:case OpCode::Greater:case OpCode::GreaterEqual:{
   if(s.stack.size()<2)return fail("QVM comparison instruction underflows stack");
   const auto rhs=s.stack.back(),lhs=s.stack[s.stack.size()-2];
   if(lhs.type!=ValueType::Number||rhs.type!=ValueType::Number)return fail("QVM ordering comparisons require numbers");
   StackValue tmp{};pop(tmp);pop(tmp);if(!push(ValueType::Boolean))return fail("QVM live string memory exceeds 16 MiB");break;
  }
  case OpCode::Print:{StackValue v{};if(!pop(v))return fail("QVM PRINT underflows stack");break;}
  case OpCode::Jump:falls=false;if(!enqueue(x.slot,s,pc))return false;break;
  case OpCode::JumpIfFalse:{
   StackValue v{};if(!pop(v)||v.type!=ValueType::Boolean)return fail("QVM conditional jump requires boolean");
   if(!enqueue(x.slot,s,pc))return false;break;
  }
  case OpCode::Halt:
   if(pc+1!=b.code.size()||!s.stack.empty())return fail("QVM HALT must be final with empty operand stack");
   reachedHalt=true;falls=false;break;
  default:return fail("QVM contains invalid opcode");
  }
  if(s.stack.size()>MaxLocals)return fail("QVM operand stack exceeds 65,536 values");
  if(liveBytes(s)>MaxLive)return fail("QVM live string memory exceeds 16 MiB");
  if(falls){
   if(pc+1>=b.code.size())return fail("QVM execution falls off end without HALT");
   if(!enqueue(pc+1,s,pc))return false;
  }
 }
 if(!reachedHalt)return fail("QVM final HALT is unreachable");
 return true;
}

bool Bytecode::save(std::ostream& out,std::string& error)const{
 if(!verifyBytecode(*this,error))return false;
 const std::array<char,8> v1={'T','V','Q','V','M','0','1','\0'};
 const std::array<char,8> v2={'T','V','Q','V','M','0','2','\0'};
 out.write((formatVersion==1?v1:v2).data(),8);
 putU32(out,static_cast<std::uint32_t>(code.size()));putU32(out,static_cast<std::uint32_t>(locals.size()));
 for(const auto& s:locals)if(!putString(out,s)){error="failed to write QVM local table";return false;}
 for(const auto& x:code){
  out.put(static_cast<char>(x.op));putU32(out,x.slot);
  const auto bits=std::bit_cast<std::uint64_t>(x.number);
  for(int n=0;n<8;++n)out.put(static_cast<char>((bits>>(n*8))&0xff));
  if(!putString(out,x.text)){error="failed to write QVM instruction text";return false;}
 }
 if(!out){error="failed to write QVM bytecode";return false;}return true;
}
bool Bytecode::load(std::istream& in,Bytecode& out,std::string& error){
 std::array<char,8> magic{};if(!in.read(magic.data(),8)){error="truncated QVM header";return false;}
 const std::array<char,8> v1={'T','V','Q','V','M','0','1','\0'};
 const std::array<char,8> v2={'T','V','Q','V','M','0','2','\0'};
 if(magic!=v1&&magic!=v2){error="invalid QVM magic or unsupported version";return false;}
 Bytecode tmp;tmp.formatVersion=magic==v1?1u:2u;
 std::uint32_t count{},localCount{};
 if(!getU32(in,count)||!getU32(in,localCount)||count>MaxInstructions||localCount>MaxLocals){
  error="invalid QVM section sizes";return false;
 }
 std::size_t total=0;
 for(std::uint32_t n=0;n<localCount;++n){
  std::string s;if(!getString(in,s,total)){error="invalid QVM local-name table";return false;}
  tmp.locals.push_back(std::move(s));
 }
 for(std::uint32_t n=0;n<count;++n){
  const int raw=in.get();std::uint32_t slot{};
  if(raw<1||!validOpcode(static_cast<std::uint8_t>(raw),tmp.formatVersion)||!getU32(in,slot)){
   error="invalid or version-incompatible QVM instruction";return false;
  }
  std::uint64_t bits=0;
  for(int k=0;k<8;++k){const int c=in.get();if(c==EOF){error="truncated QVM numeric operand";return false;}
   bits|=static_cast<std::uint64_t>(static_cast<unsigned char>(c))<<(k*8);}
  std::string text;if(!getString(in,text,total)){error="invalid QVM text operand";return false;}
  tmp.code.push_back({static_cast<OpCode>(raw),std::bit_cast<double>(bits),slot,std::move(text)});
 }
 if(in.peek()!=EOF){error="QVM file contains trailing data after final instruction";return false;}
 if(!verifyBytecode(tmp,error))return false;
 out=std::move(tmp);return true;
}
std::string disassemble(const Bytecode& b){
 std::string out;
 for(std::size_t pc=0;pc<b.code.size();++pc){
  const auto& x=b.code[pc];out+=std::to_string(pc)+"  "+name(x.op);
  if(x.op==OpCode::PushNumber)out+=" "+std::to_string(x.number);
  if(x.op==OpCode::PushString)out+=" \""+x.text+"\"";
  if(x.op==OpCode::LoadLocal||x.op==OpCode::StoreLocal||x.op==OpCode::StoreLocalMutable||
     x.op==OpCode::Jump||x.op==OpCode::JumpIfFalse)out+=" "+std::to_string(x.slot);
  if(x.op==OpCode::PushBoolean)out+=x.slot?" true":" false";
  out+='\n';
 }
 return out;
}
}
