#include "taviq/native.hpp"
#include <cstdlib>
#include <iomanip>
#include <sstream>
#include <unordered_map>
namespace taviq {
namespace {
enum class Type{Number,String,Boolean};
bool getType(const std::shared_ptr<Expr>& e,const std::unordered_map<std::string,Type>& vars,Type& t,std::vector<std::string>& errors){
 if(!e){errors.push_back("missing expression");return false;}
 if(e->kind==Expr::Kind::Number){t=Type::Number;return true;}
 if(e->kind==Expr::Kind::String){t=Type::String;return true;}
 if(e->kind==Expr::Kind::Boolean){t=Type::Boolean;return true;}
 if(e->kind==Expr::Kind::Variable){auto i=vars.find(e->text);if(i==vars.end()){errors.push_back("undefined variable: "+e->text);return false;}t=i->second;return true;}
 if(e->kind==Expr::Kind::Call){
  if(e->arguments.size()!=1){errors.push_back("native C backend: built-in requires one argument");return false;}
  Type a;if(!getType(e->arguments[0],vars,a,errors))return false;
  if(e->text=="len"&&a==Type::String){t=Type::Number;return true;}
  if((e->text=="sqrt"||e->text=="abs")&&a==Type::Number){t=Type::Number;return true;}
  errors.push_back("native C backend: unsupported built-in or argument type: "+e->text);return false;
 }
 if(e->kind==Expr::Kind::Unary){
  Type a;if(!getType(e->right,vars,a,errors))return false;
  if(e->op=='-'&&a==Type::Number){t=Type::Number;return true;}
  if(e->op=='!'&&a==Type::Boolean){t=Type::Boolean;return true;}
  errors.push_back("native C backend: invalid unary operand");return false;
 }
 Type a,b;if(!getType(e->left,vars,a,errors)||!getType(e->right,vars,b,errors))return false;
 if(e->op=='+'&&a==Type::String&&b==Type::String){errors.push_back("native C backend does not yet lower string concatenation");return false;}
 if(e->op=='&'||e->op=='|'){if(a==Type::Boolean&&b==Type::Boolean){t=Type::Boolean;return true;}}
 if(e->op=='q'||e->op=='!'){if(a==b){t=Type::Boolean;return true;}}
 if(e->op=='<'||e->op=='l'||e->op=='>'||e->op=='g'){if(a==Type::Number&&b==Type::Number){t=Type::Boolean;return true;}}
 if(a!=Type::Number||b!=Type::Number){errors.push_back("native C backend: arithmetic requires numbers");return false;}
 t=Type::Number;return true;
}
std::string quoteC(const std::string& s){
 std::ostringstream o;o<<'"';for(unsigned char c:s){switch(c){
 case '\\':o<<"\\\\";break;case '"':o<<"\\\"";break;case '\n':o<<"\\n";break;case '\r':o<<"\\r";break;case '\t':o<<"\\t";break;
 default:if(c<32||c>=127)o<<"\\"<<std::oct<<std::setw(3)<<std::setfill('0')<<static_cast<int>(c)<<std::dec;else o<<static_cast<char>(c);
 }}o<<'"';return o.str();
}
bool numeric(const std::shared_ptr<Expr>& e,const std::unordered_map<std::string,std::string>& vars,std::string& out,std::vector<std::string>& errors){
 if(e->kind==Expr::Kind::Number){char* end=nullptr;std::strtod(e->text.c_str(),&end);if(!end||*end!='\0'){errors.push_back("invalid number literal");return false;}out=e->text;return true;}
 if(e->kind==Expr::Kind::Variable){auto i=vars.find(e->text);if(i==vars.end()){errors.push_back("undefined variable: "+e->text);return false;}out=i->second;return true;}
 if(e->kind==Expr::Kind::Unary&&e->op=='-'){std::string r;if(!numeric(e->right,vars,r,errors))return false;out="(-("+r+"))";return true;}
 if(e->kind==Expr::Kind::Binary){
  std::string l,r;if(!numeric(e->left,vars,l,errors)||!numeric(e->right,vars,r,errors))return false;
  const char op=e->op; if(op!='+'&&op!='-'&&op!='*'&&op!='/'){errors.push_back("native C backend: expression is not numeric");return false;}
  out="(("+l+") "+op+" ("+r+"))";return true;
 }
 if(e->kind==Expr::Kind::Call&&e->arguments.size()==1){
  if(e->text=="len"&&e->arguments[0]->kind==Expr::Kind::String){out="((double)strlen("+quoteC(e->arguments[0]->text)+"))";return true;}
  std::string arg;if(!numeric(e->arguments[0],vars,arg,errors))return false;
  if(e->text=="sqrt"){out="sqrt("+arg+")";return true;}if(e->text=="abs"){out="fabs("+arg+")";return true;}
 }
 errors.push_back("native C backend: expression is not supported by native lowering");return false;
}
}
bool emitC(const Program& p,std::string& output,std::vector<std::string>& errors){
 if(!p.functions.empty()){
  errors.push_back("native C backend does not yet lower user-defined functions");
  return false;
 }
 std::ostringstream c;c<<"#include <stdio.h>\n#include <math.h>\n#include <string.h>\nint main(void) {\n";
 std::unordered_map<std::string,std::string> vars;
 std::unordered_map<std::string,Type> types;
 std::unordered_map<std::string,bool> mutables;
 std::size_t next=0;
 for(const auto& s:p.statements){
  if(s.kind==Statement::Kind::Let||s.kind==Statement::Kind::Var){
   if(vars.contains(s.name)){errors.push_back("duplicate variable: "+s.name);continue;}
   Type t;if(!getType(s.expression,types,t,errors))continue;
   if(!s.declaredType.empty()){
    Type annotated=Type::Number;
    if(s.declaredType=="number")annotated=Type::Number;
    else if(s.declaredType=="string")annotated=Type::String;
    else if(s.declaredType=="bool")annotated=Type::Boolean;
    else{errors.push_back("native C backend: unknown type annotation '"+s.declaredType+"'");continue;}
    if(t!=annotated){errors.push_back("native C backend: initializer does not match declared type '"+s.declaredType+"'");continue;}
   }
   if(t!=Type::Number){errors.push_back("native C backend currently lowers numeric variables only");continue;}
   std::string expr;if(!numeric(s.expression,vars,expr,errors))continue;
   const std::string name="taviq_v"+std::to_string(next++);
   c<<"  double "<<name<<" = "<<expr<<";\n";vars.emplace(s.name,name);types.emplace(s.name,t);
   mutables.emplace(s.name,s.kind==Statement::Kind::Var);
  }else if(s.kind==Statement::Kind::Assign){
   auto v=vars.find(s.name);auto t=types.find(s.name);
   if(v==vars.end()||t==types.end()){errors.push_back("native C backend: assignment to unknown variable "+s.name);continue;}
   if(!mutables[s.name]){errors.push_back("native C backend: cannot assign to immutable let variable "+s.name);continue;}
   Type actual;if(!getType(s.expression,types,actual,errors)||actual!=t->second){errors.push_back("native C backend: assignment type mismatch");continue;}
   std::string expr;if(numeric(s.expression,vars,expr,errors))c<<"  "<<v->second<<" = "<<expr<<";\n";
  }else if(s.kind==Statement::Kind::Display){
   if(s.expression&&s.expression->kind==Expr::Kind::String)c<<"  puts("<<quoteC(s.expression->text)<<");\n";
   else{
    Type t;if(!getType(s.expression,types,t,errors))continue;
    if(t!=Type::Number){errors.push_back("native C backend currently displays numeric expressions and string literals only");continue;}
    std::string expr;if(numeric(s.expression,vars,expr,errors))c<<"  printf(\"%.15g\\n\", (double)("<<expr<<"));\n";
   }
  }else errors.push_back("native C backend does not yet lower if/else or while control flow");
 }
 if(!errors.empty())return false;
 c<<"  return 0;\n}\n";output=c.str();return true;
}
}
