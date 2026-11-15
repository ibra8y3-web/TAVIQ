#include "taviq/vm.hpp"
#include <ostream>
#include <utility>
#include <variant>
#include <vector>
#include <cmath>
namespace taviq {
int VM::run(const Bytecode& b,std::ostream& output,std::string& error,std::size_t budget)const{
 using Value=std::variant<double,std::string>;std::vector<Value> stack;
 if(!verifyBytecode(b,error))return 1;
 std::vector<Value> locals(b.locals.size());std::vector<bool> initialized(b.locals.size(),false);
 auto fail=[&](const std::string& m){error=m;return 1;};
 auto pop=[&](Value& v)->bool{if(stack.empty())return false;v=std::move(stack.back());stack.pop_back();return true;};
 if(b.code.size()>budget)return fail("QVM program exceeds instruction budget");
 for(std::size_t pc=0;pc<b.code.size();++pc){const auto& i=b.code[pc];switch(i.op){
 case OpCode::PushNumber:if(!std::isfinite(i.number))return fail("QVM invalid non-finite number");stack.emplace_back(i.number);break;
 case OpCode::PushString:stack.emplace_back(i.text);break;
 case OpCode::LoadLocal:if(i.slot>=locals.size()||!initialized[i.slot])return fail("QVM invalid or uninitialized local slot");stack.push_back(locals[i.slot]);break;
 case OpCode::StoreLocal:{Value v;if(i.slot>=locals.size()||!pop(v))return fail("QVM stack underflow or invalid local slot");locals[i.slot]=std::move(v);initialized[i.slot]=true;break;}
 case OpCode::Negate:{Value a;if(!pop(a)||!std::holds_alternative<double>(a))return fail("QVM NEG expects a number");stack.emplace_back(-std::get<double>(a));break;}
 case OpCode::Length:{Value a;if(!pop(a)||!std::holds_alternative<std::string>(a))return fail("QVM LEN expects a string");stack.emplace_back(static_cast<double>(std::get<std::string>(a).size()));break;}
 case OpCode::SquareRoot:{Value a;if(!pop(a)||!std::holds_alternative<double>(a))return fail("QVM SQRT expects a number");double v=std::sqrt(std::get<double>(a));if(!std::isfinite(v))return fail("QVM SQRT input must be non-negative and finite");stack.emplace_back(v);break;}
 case OpCode::Absolute:{Value a;if(!pop(a)||!std::holds_alternative<double>(a))return fail("QVM ABS expects a number");stack.emplace_back(std::fabs(std::get<double>(a)));break;}
 case OpCode::Add:case OpCode::Subtract:case OpCode::Multiply:case OpCode::Divide:{Value rhs,lhs;if(!pop(rhs)||!pop(lhs))return fail("QVM arithmetic stack underflow");if(i.op==OpCode::Add&&std::holds_alternative<std::string>(lhs)&&std::holds_alternative<std::string>(rhs)){stack.emplace_back(std::get<std::string>(lhs)+std::get<std::string>(rhs));break;}if(!std::holds_alternative<double>(lhs)||!std::holds_alternative<double>(rhs))return fail("QVM arithmetic operands must be numbers");double a=std::get<double>(lhs),c=std::get<double>(rhs),v=0;if(i.op==OpCode::Add)v=a+c;else if(i.op==OpCode::Subtract)v=a-c;else if(i.op==OpCode::Multiply)v=a*c;else{if(c==0.0)return fail("division by zero");v=a/c;}if(!std::isfinite(v))return fail("numeric result is not finite");stack.emplace_back(v);break;}
 case OpCode::Print:{Value v;if(!pop(v))return fail("QVM PRINT stack underflow");if(std::holds_alternative<double>(v))output<<std::get<double>(v);else output<<std::get<std::string>(v);output<<'\n';if(!output)return fail("failed to write program output");break;}
 case OpCode::Halt:return 0;
 default:return fail("unknown opcode in QVM");
 }if(stack.size()>65536)return fail("QVM stack limit exceeded");}
 return fail("QVM bytecode has no HALT instruction");
}
}