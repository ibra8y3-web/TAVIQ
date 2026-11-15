#include "taviq/vm.hpp"
#include <cmath>
#include <ostream>
#include <utility>
#include <variant>
#include <vector>
namespace taviq {
int VM::run(const Bytecode& b,std::ostream& output,std::string& error,std::size_t budget)const{
 error.clear();if(!verifyBytecode(b,error))return 1;
 using Value=std::variant<double,std::string,bool>;
 std::vector<Value> stack;std::vector<Value> locals(b.locals.size());
 std::vector<bool> initialized(b.locals.size(),false),mutableLocals(b.locals.size(),false);
 constexpr std::size_t MaxStringBytes=16*1024*1024;
 const auto fail=[&](const std::string& m){error=m;return 1;};
 const auto pop=[&](Value& value){if(stack.empty())return false;value=std::move(stack.back());stack.pop_back();return true;};
 std::size_t pc=0,steps=0;
 while(pc<b.code.size()){
  if(steps++>=budget)return fail("QVM execution instruction budget exceeded");
  const auto& x=b.code[pc];bool advancePc=true;
  switch(x.op){
   case OpCode::PushNumber:
    if(!std::isfinite(x.number))return fail("QVM invalid non-finite number");
    stack.emplace_back(x.number);break;
   case OpCode::PushString:stack.emplace_back(x.text);break;
   case OpCode::PushBoolean:stack.emplace_back(x.slot!=0);break;
   case OpCode::LoadLocal:
    if(x.slot>=locals.size()||!initialized[x.slot])return fail("QVM invalid or uninitialized local slot");
    stack.push_back(locals[x.slot]);break;
   case OpCode::ResetLocal:
    if(x.slot>=locals.size())return fail("QVM RESET_LOCAL references invalid local slot");
    locals[x.slot]=0.0;initialized[x.slot]=false;mutableLocals[x.slot]=false;break;
   case OpCode::StoreLocal:
   case OpCode::StoreLocalMutable:{
    Value value;if(x.slot>=locals.size()||!pop(value))return fail("QVM invalid local store or stack underflow");
    if(x.op==OpCode::StoreLocal){
     if(initialized[x.slot])return fail("QVM immutable local is initialized more than once");
     mutableLocals[x.slot]=false;
    }else{
     if(initialized[x.slot]&&!mutableLocals[x.slot])return fail("QVM mutable store overwrites immutable local");
     mutableLocals[x.slot]=true;
     if(initialized[x.slot]&&locals[x.slot].index()!=value.index())return fail("QVM mutable local assignment changes type");
    }
    locals[x.slot]=std::move(value);initialized[x.slot]=true;break;
   }
   case OpCode::Negate:case OpCode::SquareRoot:case OpCode::Absolute:{
    Value value;if(!pop(value)||!std::holds_alternative<double>(value))return fail("QVM numeric unary operation requires number");
    const double n=std::get<double>(value);double result=n;
    if(x.op==OpCode::Negate)result=-n;
    else if(x.op==OpCode::SquareRoot){if(n<0.0)return fail("QVM SQRT input must be non-negative");result=std::sqrt(n);}
    else result=std::fabs(n);
    if(!std::isfinite(result))return fail("QVM numeric unary result is not finite");
    stack.emplace_back(result);break;
   }
   case OpCode::LogicalNot:{
    Value value;if(!pop(value)||!std::holds_alternative<bool>(value))return fail("QVM NOT expects boolean");
    stack.emplace_back(!std::get<bool>(value));break;
   }
   case OpCode::Length:{
    Value value;if(!pop(value)||!std::holds_alternative<std::string>(value))return fail("QVM LENGTH expects string");
    stack.emplace_back(static_cast<double>(std::get<std::string>(value).size()));break;
   }
   case OpCode::Add:case OpCode::Subtract:case OpCode::Multiply:case OpCode::Divide:{
    if(stack.size()<2)return fail("QVM arithmetic stack underflow");
    Value rhs=std::move(stack.back());stack.pop_back();Value lhs=std::move(stack.back());stack.pop_back();
    if(x.op==OpCode::Add&&std::holds_alternative<std::string>(lhs)&&std::holds_alternative<std::string>(rhs)){
     const auto& a=std::get<std::string>(lhs);const auto& c=std::get<std::string>(rhs);
     if(a.size()>MaxStringBytes||c.size()>MaxStringBytes-a.size())return fail("QVM string concatenation exceeds 16 MiB");
     std::string joined;joined.reserve(a.size()+c.size());joined.append(a);joined.append(c);stack.emplace_back(std::move(joined));break;
    }
    if(!std::holds_alternative<double>(lhs)||!std::holds_alternative<double>(rhs))return fail("QVM arithmetic operands must be numbers");
    const double a=std::get<double>(lhs),c=std::get<double>(rhs);double result=0.0;
    if(x.op==OpCode::Add)result=a+c;else if(x.op==OpCode::Subtract)result=a-c;
    else if(x.op==OpCode::Multiply)result=a*c;else{if(c==0.0)return fail("division by zero");result=a/c;}
    if(!std::isfinite(result))return fail("numeric result is not finite");stack.emplace_back(result);break;
   }
   case OpCode::Equal:case OpCode::NotEqual:{
    if(stack.size()<2)return fail("QVM equality stack underflow");
    Value rhs=std::move(stack.back());stack.pop_back();Value lhs=std::move(stack.back());stack.pop_back();
    const bool equal=lhs==rhs;stack.emplace_back(x.op==OpCode::Equal?equal:!equal);break;
   }
   case OpCode::Less:case OpCode::LessEqual:case OpCode::Greater:case OpCode::GreaterEqual:{
    if(stack.size()<2)return fail("QVM comparison stack underflow");
    Value rhs=std::move(stack.back());stack.pop_back();Value lhs=std::move(stack.back());stack.pop_back();
    if(!std::holds_alternative<double>(lhs)||!std::holds_alternative<double>(rhs))return fail("QVM ordering comparisons require numbers");
    const double a=std::get<double>(lhs),c=std::get<double>(rhs);bool result=false;
    if(x.op==OpCode::Less)result=a<c;else if(x.op==OpCode::LessEqual)result=a<=c;
    else if(x.op==OpCode::Greater)result=a>c;else result=a>=c;
    stack.emplace_back(result);break;
   }
   case OpCode::Print:{
    Value value;if(!pop(value))return fail("QVM PRINT stack underflow");
    if(std::holds_alternative<double>(value))output<<std::get<double>(value);
    else if(std::holds_alternative<std::string>(value))output<<std::get<std::string>(value);
    else output<<(std::get<bool>(value)?"true":"false");
    output<<'\n';if(!output)return fail("failed to write program output");break;
   }
   case OpCode::Jump:pc=x.slot;advancePc=false;break;
   case OpCode::JumpIfFalse:{
    Value value;if(!pop(value)||!std::holds_alternative<bool>(value))return fail("QVM conditional jump expects boolean");
    if(!std::get<bool>(value)){pc=x.slot;advancePc=false;}break;
   }
   case OpCode::Halt:return 0;
   default:return fail("unknown opcode in QVM");
  }
  if(stack.size()>65'536)return fail("QVM operand stack limit exceeded");
  if(advancePc)++pc;
 }
 return fail("QVM execution left the instruction stream without HALT");
}
}
