#include "taviq/compiler.hpp"
#include <cmath>
#include <cstdlib>
#include <unordered_map>
namespace taviq {
namespace {
enum class Type { Number, String };
bool infer(const std::shared_ptr<Expr>& e,const std::unordered_map<std::string,Type>& types,Type& result,std::vector<std::string>& errors){
 if(!e){errors.push_back("missing expression");return false;}
 if(e->kind==Expr::Kind::Number){result=Type::Number;return true;}
 if(e->kind==Expr::Kind::String){result=Type::String;return true;}
 if(e->kind==Expr::Kind::Variable){auto it=types.find(e->text);if(it==types.end()){errors.push_back("undefined variable: "+e->text);return false;}result=it->second;return true;}
 if(e->kind==Expr::Kind::Call){if(e->arguments.size()!=1){errors.push_back("built-in function '"+e->text+"' requires exactly one argument");return false;}Type argument;if(!infer(e->arguments.front(),types,argument,errors))return false;if(e->text=="len"&&argument==Type::String){result=Type::Number;return true;}if((e->text=="sqrt"||e->text=="abs")&&argument==Type::Number){result=Type::Number;return true;}if(e->text!="len"&&e->text!="sqrt"&&e->text!="abs"){errors.push_back("unknown built-in function: "+e->text);return false;}errors.push_back("invalid argument type for built-in function '"+e->text+"'");return false;}
 if(e->kind==Expr::Kind::Unary){Type t;if(!infer(e->right,types,t,errors))return false;if(t!=Type::Number){errors.push_back("unary '-' requires a number");return false;}result=Type::Number;return true;}
 Type l,r;if(!infer(e->left,types,l,errors)||!infer(e->right,types,r,errors))return false;if(e->op=='+'&&l==Type::String&&r==Type::String){result=Type::String;return true;}if(l!=Type::Number||r!=Type::Number){errors.push_back("arithmetic operators require numbers (only string + string is supported)");return false;}result=Type::Number;return true;
}
bool emitExpr(const std::shared_ptr<Expr>& e,Bytecode& b,const std::unordered_map<std::string,std::uint32_t>& vars,const std::unordered_map<std::string,Type>& types,std::vector<std::string>& errors){
 Type t;if(!infer(e,types,t,errors))return false;auto push=[&](OpCode op){b.code.push_back(Instruction{op,0,0,{}});};
 switch(e->kind){
 case Expr::Kind::Number:{char* end=nullptr;double v=std::strtod(e->text.c_str(),&end);if(!end||*end!='\0'||!std::isfinite(v)){errors.push_back("invalid or non-finite number literal: "+e->text);return false;}b.code.push_back(Instruction{OpCode::PushNumber,v,0,{}});return true;}
 case Expr::Kind::String:b.code.push_back(Instruction{OpCode::PushString,0,0,e->text});return true;
 case Expr::Kind::Variable:{auto it=vars.find(e->text);if(it==vars.end()){errors.push_back("undefined variable: "+e->text);return false;}b.code.push_back(Instruction{OpCode::LoadLocal,0,it->second,{}});return true;}
 case Expr::Kind::Call:if(!emitExpr(e->arguments.front(),b,vars,types,errors))return false;if(e->text=="len")push(OpCode::Length);else if(e->text=="sqrt")push(OpCode::SquareRoot);else if(e->text=="abs")push(OpCode::Absolute);return true;
 case Expr::Kind::Unary:if(!emitExpr(e->right,b,vars,types,errors))return false;push(OpCode::Negate);return true;
 case Expr::Kind::Binary:if(!emitExpr(e->left,b,vars,types,errors)||!emitExpr(e->right,b,vars,types,errors))return false;if(e->op=='+')push(OpCode::Add);else if(e->op=='-')push(OpCode::Subtract);else if(e->op=='*')push(OpCode::Multiply);else if(e->op=='/')push(OpCode::Divide);else{errors.push_back("unsupported binary operator");return false;}return true;
 }
 return false;
}
}
bool compile(const Program& p,Bytecode& out,std::vector<std::string>& errors){Bytecode b;std::unordered_map<std::string,std::uint32_t> vars;std::unordered_map<std::string,Type> types;for(const auto& s:p.statements){if(s.kind==Statement::Kind::Let){if(vars.contains(s.name)){errors.push_back("line "+std::to_string(s.line)+": variable already declared: "+s.name);continue;}Type type;if(!infer(s.expression,types,type,errors)||!emitExpr(s.expression,b,vars,types,errors))continue;if(b.locals.size()>=65536||b.code.size()>=1000000){errors.push_back("program exceeds QVM v1 limits");break;}const auto slot=static_cast<std::uint32_t>(b.locals.size());b.locals.push_back(s.name);vars.emplace(s.name,slot);types.emplace(s.name,type);b.code.push_back(Instruction{OpCode::StoreLocal,0,slot,{}});}else if(emitExpr(s.expression,b,vars,types,errors))b.code.push_back(Instruction{OpCode::Print,0,0,{}});}if(!errors.empty())return false;if(b.code.size()>=1000000){errors.push_back("program exceeds QVM instruction limit");return false;}b.code.push_back(Instruction{OpCode::Halt,0,0,{}});out=std::move(b);return true;}
}