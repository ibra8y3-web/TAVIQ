#include "taviq/compiler.hpp"
#include <cmath>
#include <cstdlib>
#include <unordered_map>
namespace taviq {
namespace {
enum class Type{Number,String,Boolean};
struct Binding{std::uint32_t slot{};Type type{Type::Number};bool mutableLocal{false};};
struct Context{
 Bytecode& bytecode;
 std::vector<std::unordered_map<std::string,Binding>> scopes{{}};
};
const Binding* resolve(const Context& c,const std::string& name){
 for(auto it=c.scopes.rbegin();it!=c.scopes.rend();++it){
  const auto found=it->find(name);if(found!=it->end())return &found->second;
 }
 return nullptr;
}
bool infer(const std::shared_ptr<Expr>& e,const Context& c,Type& result,std::vector<std::string>& errors){
 if(!e){errors.push_back("missing expression");return false;}
 if(e->kind==Expr::Kind::Number){result=Type::Number;return true;}
 if(e->kind==Expr::Kind::String){result=Type::String;return true;}
 if(e->kind==Expr::Kind::Boolean){result=Type::Boolean;return true;}
 if(e->kind==Expr::Kind::Variable){
  const auto* b=resolve(c,e->text);
  if(!b){errors.push_back("undefined variable: "+e->text);return false;}
  result=b->type;return true;
 }
 if(e->kind==Expr::Kind::Call){
  if(e->arguments.size()!=1){errors.push_back("built-in function '"+e->text+"' requires exactly one argument");return false;}
  Type a;if(!infer(e->arguments.front(),c,a,errors))return false;
  if(e->text=="len"&&a==Type::String){result=Type::Number;return true;}
  if((e->text=="sqrt"||e->text=="abs")&&a==Type::Number){result=Type::Number;return true;}
  if(e->text!="len"&&e->text!="sqrt"&&e->text!="abs"){errors.push_back("unknown built-in function: "+e->text);return false;}
  errors.push_back("invalid argument type for built-in function '"+e->text+"'");return false;
 }
 if(e->kind==Expr::Kind::Unary){
  Type a;if(!infer(e->right,c,a,errors))return false;
  if(e->op=='-'&&a==Type::Number){result=Type::Number;return true;}
  if(e->op=='!'&&a==Type::Boolean){result=Type::Boolean;return true;}
  errors.push_back(e->op=='-'?"unary '-' requires a number":"logical NOT requires a boolean");return false;
 }
 Type a,b;
 if(!infer(e->left,c,a,errors)||!infer(e->right,c,b,errors))return false;
 if(e->op=='+'&&a==Type::String&&b==Type::String){result=Type::String;return true;}
 if(e->op=='&'||e->op=='|'){
  if(a==Type::Boolean&&b==Type::Boolean){result=Type::Boolean;return true;}
  errors.push_back("logical AND/OR requires boolean operands");return false;
 }
 if(e->op=='q'||e->op=='!'){
  if(a==b){result=Type::Boolean;return true;}
  errors.push_back("equality operands must have the same type");return false;
 }
 if(e->op=='<'||e->op=='l'||e->op=='>'||e->op=='g'){
  if(a==Type::Number&&b==Type::Number){result=Type::Boolean;return true;}
  errors.push_back("ordering comparisons require numeric operands");return false;
 }
 if(a!=Type::Number||b!=Type::Number){
  errors.push_back("arithmetic operators require numbers (only string + string is supported)");return false;
 }
 result=Type::Number;return true;
}
void emit(Context& c,OpCode op,std::uint32_t slot=0,double number=0.0,std::string text={}){
 c.bytecode.code.push_back(Instruction{op,number,slot,std::move(text)});
}
bool emitExpr(const std::shared_ptr<Expr>& e,Context& c,std::vector<std::string>& errors){
 Type type;if(!infer(e,c,type,errors))return false;
 switch(e->kind){
  case Expr::Kind::Number:{
   char* end=nullptr;const double value=std::strtod(e->text.c_str(),&end);
   if(!end||*end!='\0'||!std::isfinite(value)){errors.push_back("invalid or non-finite number literal: "+e->text);return false;}
   emit(c,OpCode::PushNumber,0,value);return true;
  }
  case Expr::Kind::String:emit(c,OpCode::PushString,0,0.0,e->text);return true;
  case Expr::Kind::Boolean:emit(c,OpCode::PushBoolean,e->boolean?1u:0u);return true;
  case Expr::Kind::Variable:{
   const auto* b=resolve(c,e->text);if(!b){errors.push_back("undefined variable: "+e->text);return false;}
   emit(c,OpCode::LoadLocal,b->slot);return true;
  }
  case Expr::Kind::Call:
   if(!emitExpr(e->arguments.front(),c,errors))return false;
   if(e->text=="len")emit(c,OpCode::Length);
   else if(e->text=="sqrt")emit(c,OpCode::SquareRoot);
   else if(e->text=="abs")emit(c,OpCode::Absolute);
   return true;
  case Expr::Kind::Unary:
   if(!emitExpr(e->right,c,errors))return false;
   emit(c,e->op=='-'?OpCode::Negate:OpCode::LogicalNot);return true;
  case Expr::Kind::Binary:
   if(e->op=='&'){
    if(!emitExpr(e->left,c,errors))return false;
    const auto branch=c.bytecode.code.size();emit(c,OpCode::JumpIfFalse);
    if(!emitExpr(e->right,c,errors))return false;
    const auto done=c.bytecode.code.size();emit(c,OpCode::Jump);
    c.bytecode.code[branch].slot=static_cast<std::uint32_t>(c.bytecode.code.size());
    emit(c,OpCode::PushBoolean,0);
    c.bytecode.code[done].slot=static_cast<std::uint32_t>(c.bytecode.code.size());
    return true;
   }
   if(e->op=='|'){
    if(!emitExpr(e->left,c,errors))return false;
    const auto branch=c.bytecode.code.size();emit(c,OpCode::JumpIfFalse);
    emit(c,OpCode::PushBoolean,1);
    const auto done=c.bytecode.code.size();emit(c,OpCode::Jump);
    c.bytecode.code[branch].slot=static_cast<std::uint32_t>(c.bytecode.code.size());
    if(!emitExpr(e->right,c,errors))return false;
    c.bytecode.code[done].slot=static_cast<std::uint32_t>(c.bytecode.code.size());
    return true;
   }
   if(!emitExpr(e->left,c,errors)||!emitExpr(e->right,c,errors))return false;
   switch(e->op){
    case '+':emit(c,OpCode::Add);break;case '-':emit(c,OpCode::Subtract);break;
    case '*':emit(c,OpCode::Multiply);break;case '/':emit(c,OpCode::Divide);break;
    case 'q':emit(c,OpCode::Equal);break;case '!':emit(c,OpCode::NotEqual);break;
    case '<':emit(c,OpCode::Less);break;case 'l':emit(c,OpCode::LessEqual);break;
    case '>':emit(c,OpCode::Greater);break;case 'g':emit(c,OpCode::GreaterEqual);break;
    default:errors.push_back("unsupported binary operator");return false;
   }
   return true;
 }
 return false;
}
bool compileStatements(const std::vector<Statement>& statements,Context& c,
                       std::vector<std::string>& errors,bool insideLoop);
bool compileBlock(const std::vector<Statement>& statements,Context& c,
                  std::vector<std::string>& errors,bool insideLoop){
 c.scopes.emplace_back();
 const bool ok=compileStatements(statements,c,errors,insideLoop);
 c.scopes.pop_back();
 return ok;
}
bool compileStatements(const std::vector<Statement>& statements,Context& c,
                       std::vector<std::string>& errors,bool insideLoop){
 bool ok=true;
 for(const auto& s:statements){
  if(s.kind==Statement::Kind::Let||s.kind==Statement::Kind::Var){
   if(insideLoop&&s.kind==Statement::Kind::Let){
    errors.push_back("line "+std::to_string(s.line)+": use 'var' for declarations inside a while loop; loop-local let bindings are not supported yet");
    ok=false;continue;
   }
   if(c.scopes.back().contains(s.name)){
    errors.push_back("line "+std::to_string(s.line)+": variable already declared in this scope: "+s.name);ok=false;continue;
   }
   Type type;
   if(!infer(s.expression,c,type,errors)||!emitExpr(s.expression,c,errors)){ok=false;continue;}
   if(c.bytecode.locals.size()>=65'536){errors.push_back("QVM local limit exceeded");return false;}
   const auto slot=static_cast<std::uint32_t>(c.bytecode.locals.size());
   c.bytecode.locals.push_back(s.name+"#"+std::to_string(slot));
   const bool mutableLocal=s.kind==Statement::Kind::Var;
   c.scopes.back().emplace(s.name,Binding{slot,type,mutableLocal});
   emit(c,mutableLocal?OpCode::StoreLocalMutable:OpCode::StoreLocal,slot);
  }else if(s.kind==Statement::Kind::Assign){
   const auto* binding=resolve(c,s.name);
   if(!binding){errors.push_back("line "+std::to_string(s.line)+": assignment to undefined variable: "+s.name);ok=false;continue;}
   if(!binding->mutableLocal){errors.push_back("line "+std::to_string(s.line)+": cannot assign to immutable let variable: "+s.name);ok=false;continue;}
   Type type;
   if(!infer(s.expression,c,type,errors)||type!=binding->type){
    if(errors.empty()||errors.back().find("assignment type") == std::string::npos)
     errors.push_back("line "+std::to_string(s.line)+": assignment type must match variable '"+s.name+"'");
    ok=false;continue;
   }
   if(!emitExpr(s.expression,c,errors)){ok=false;continue;}
   emit(c,OpCode::StoreLocalMutable,binding->slot);
  }else if(s.kind==Statement::Kind::Display){
   if(!emitExpr(s.expression,c,errors)){ok=false;continue;}
   emit(c,OpCode::Print);
  }else if(s.kind==Statement::Kind::If){
   Type condition;
   if(!infer(s.expression,c,condition,errors)||condition!=Type::Boolean){
    errors.push_back("line "+std::to_string(s.line)+": if condition must be boolean");ok=false;continue;
   }
   if(!emitExpr(s.expression,c,errors)){ok=false;continue;}
   const auto otherwise=c.bytecode.code.size();emit(c,OpCode::JumpIfFalse);
   if(!compileBlock(s.body,c,errors,insideLoop))ok=false;
   if(!s.elseBody.empty()){
    const auto done=c.bytecode.code.size();emit(c,OpCode::Jump);
    c.bytecode.code[otherwise].slot=static_cast<std::uint32_t>(c.bytecode.code.size());
    if(!compileBlock(s.elseBody,c,errors,insideLoop))ok=false;
    c.bytecode.code[done].slot=static_cast<std::uint32_t>(c.bytecode.code.size());
   }else c.bytecode.code[otherwise].slot=static_cast<std::uint32_t>(c.bytecode.code.size());
  }else if(s.kind==Statement::Kind::While){
   const auto loopHead=c.bytecode.code.size();
   Type condition;
   if(!infer(s.expression,c,condition,errors)||condition!=Type::Boolean){
    errors.push_back("line "+std::to_string(s.line)+": while condition must be boolean");ok=false;continue;
   }
   if(!emitExpr(s.expression,c,errors)){ok=false;continue;}
   const auto exit=c.bytecode.code.size();emit(c,OpCode::JumpIfFalse);
   if(!compileBlock(s.body,c,errors,true))ok=false;
   emit(c,OpCode::Jump,static_cast<std::uint32_t>(loopHead));
   c.bytecode.code[exit].slot=static_cast<std::uint32_t>(c.bytecode.code.size());
  }
  if(c.bytecode.code.size()>1'000'000){errors.push_back("program exceeds QVM instruction limit");return false;}
 }
 return ok;
}
}
bool compile(const Program& program,Bytecode& output,std::vector<std::string>& errors){
 Bytecode result;result.formatVersion=2;
 Context context{result};
 const bool ok=compileStatements(program.statements,context,errors,false);
 if(!ok||!errors.empty())return false;
 if(result.code.size()>=1'000'000){errors.push_back("program exceeds QVM instruction limit");return false;}
 emit(context,OpCode::Halt);
 std::string verifyError;
 if(!verifyBytecode(result,verifyError)){errors.push_back("compiler emitted invalid QVM: "+verifyError);return false;}
 output=std::move(result);
 return true;
}
}
