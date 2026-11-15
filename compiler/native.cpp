#include "taviq/native.hpp"
#include <cmath>
#include <cstdlib>
#include <iomanip>
#include <sstream>
#include <unordered_map>
#include <utility>
namespace taviq {
namespace {
enum class Type{Number,String,Boolean};
struct Binding{std::string symbol;Type type;bool mutableLocal;};
struct Context{
 std::vector<std::unordered_map<std::string,Binding>> scopes{{}};
 std::ostringstream code;
 std::size_t nextSymbol{0};
 std::vector<std::string> errors;
};
const Binding* resolve(const Context& c,const std::string& name){
 for(auto scope=c.scopes.rbegin();scope!=c.scopes.rend();++scope){
  const auto found=scope->find(name);if(found!=scope->end())return &found->second;
 }
 return nullptr;
}
std::string quoteC(const std::string& value){
 std::ostringstream out;out<<'"';
 for(unsigned char c:value){
  switch(c){
   case '\\':out<<"\\\\";break;
   case '"':out<<"\\\"";break;
   case '\n':out<<"\\n";break;
   case '\r':out<<"\\r";break;
   case '\t':out<<"\\t";break;
   case '\0':out<<"\\000";break;
   default:
    if(c<32||c>=127)out<<"\\"<<std::oct<<std::setw(3)<<std::setfill('0')<<static_cast<int>(c)<<std::dec;
    else out<<static_cast<char>(c);
  }
 }
 out<<'"';return out.str();
}
bool infer(const std::shared_ptr<Expr>& expression,const Context& c,Type& type){
 if(!expression){const_cast<Context&>(c).errors.push_back("native C backend: missing expression");return false;}
 const auto error=[&](const std::string& message){const_cast<Context&>(c).errors.push_back(message);return false;};
 switch(expression->kind){
  case Expr::Kind::Number:type=Type::Number;return true;
  case Expr::Kind::String:type=Type::String;return true;
  case Expr::Kind::Boolean:type=Type::Boolean;return true;
  case Expr::Kind::Variable:{
   const auto* binding=resolve(c,expression->text);
   if(!binding)return error("native C backend: undefined variable "+expression->text);
   type=binding->type;return true;
  }
  case Expr::Kind::Call:{
   if(expression->arguments.size()!=1)return error("native C backend: built-in function requires exactly one argument");
   Type arg;if(!infer(expression->arguments.front(),c,arg))return false;
   if(expression->text=="len"&&arg==Type::String){type=Type::Number;return true;}
   if((expression->text=="sqrt"||expression->text=="abs")&&arg==Type::Number){type=Type::Number;return true;}
   return error("native C backend: unsupported built-in function or argument type "+expression->text);
  }
  case Expr::Kind::Unary:{
   Type arg;if(!infer(expression->right,c,arg))return false;
   if(expression->op=='-'&&arg==Type::Number){type=Type::Number;return true;}
   if(expression->op=='!'&&arg==Type::Boolean){type=Type::Boolean;return true;}
   return error("native C backend: invalid unary operand type");
  }
  case Expr::Kind::Binary:break;
 }
 Type left,right;
 if(!infer(expression->left,c,left)||!infer(expression->right,c,right))return false;
 switch(expression->op){
  case '+':
   if(left==Type::Number&&right==Type::Number){type=Type::Number;return true;}
   if(left==Type::String&&right==Type::String)
    return error("native C backend: string concatenation is not yet lowered to C");
   return error("native C backend: '+' operands must both be numbers or strings");
  case '-':case '*':case '/':
   if(left==Type::Number&&right==Type::Number){type=Type::Number;return true;}
   return error("native C backend: arithmetic requires numbers");
  case '<':case 'l':case '>':case 'g':
   if(left==Type::Number&&right==Type::Number){type=Type::Boolean;return true;}
   return error("native C backend: ordering comparisons require numbers");
  case 'q':case '!':
   if(left==right){type=Type::Boolean;return true;}
   return error("native C backend: equality operands must have the same type");
  case '&':case '|':
   if(left==Type::Boolean&&right==Type::Boolean){type=Type::Boolean;return true;}
   return error("native C backend: logical operators require booleans");
  default:return error("native C backend: unsupported binary operator");
 }
}
bool emitExpr(const std::shared_ptr<Expr>& expression,const Context& c,std::string& out){
 if(!expression)return false;
 Type result;if(!infer(expression,c,result))return false;
 switch(expression->kind){
  case Expr::Kind::Number:{
   char* end=nullptr;const double value=std::strtod(expression->text.c_str(),&end);
   if(!end||*end!='\0'||!std::isfinite(value)){
    const_cast<Context&>(c).errors.push_back("native C backend: invalid or non-finite number "+expression->text);return false;
   }
   out=expression->text;return true;
  }
  case Expr::Kind::String:out=quoteC(expression->text);return true;
  case Expr::Kind::Boolean:out=expression->boolean?"1":"0";return true;
  case Expr::Kind::Variable:{
   const auto* binding=resolve(c,expression->text);
   if(!binding){const_cast<Context&>(c).errors.push_back("native C backend: undefined variable "+expression->text);return false;}
   out=binding->symbol;return true;
  }
  case Expr::Kind::Call:{
   std::string arg;if(!emitExpr(expression->arguments.front(),c,arg))return false;
   if(expression->text=="len")out="((double)strlen("+arg+"))";
   else if(expression->text=="sqrt")out="taviq_sqrt("+arg+")";
   else if(expression->text=="abs")out="fabs("+arg+")";
   else return false;
   return true;
  }
  case Expr::Kind::Unary:{
   std::string arg;if(!emitExpr(expression->right,c,arg))return false;
   out=std::string("(")+(expression->op=='-'?"-":"!")+ "("+arg+"))";return true;
  }
  case Expr::Kind::Binary:break;
 }
 std::string left,right;
 if(!emitExpr(expression->left,c,left)||!emitExpr(expression->right,c,right))return false;
 const Type leftType=[&](){Type t;infer(expression->left,c,t);return t;}();
 switch(expression->op){
  case '+':out="(("+left+") + ("+right+"))";break;
  case '-':out="(("+left+") - ("+right+"))";break;
  case '*':out="(("+left+") * ("+right+"))";break;
  case '/':out="taviq_div("+left+","+right+")";break;
  case '<':out="(("+left+") < ("+right+"))";break;
  case 'l':out="(("+left+") <= ("+right+"))";break;
  case '>':out="(("+left+") > ("+right+"))";break;
  case 'g':out="(("+left+") >= ("+right+"))";break;
  case '&':out="(("+left+") && ("+right+"))";break;
  case '|':out="(("+left+") || ("+right+"))";break;
  case 'q':
   out=leftType==Type::String?"(strcmp("+left+","+right+")==0)":"(("+left+") == ("+right+"))";break;
  case '!':
   out=leftType==Type::String?"(strcmp("+left+","+right+")!=0)":"(("+left+") != ("+right+"))";break;
  default:return false;
 }
 return true;
}
std::string indentation(std::size_t depth){return std::string(depth*4,' ');}
bool emitStatements(const std::vector<Statement>& statements,Context& c,
                    std::size_t depth,bool insideLoop);
bool emitBlock(const std::vector<Statement>& statements,Context& c,
               std::size_t depth,bool insideLoop){
 c.scopes.emplace_back();
 c.code<<indentation(depth)<<"{\n";
 const bool ok=emitStatements(statements,c,depth+1,insideLoop);
 c.code<<indentation(depth)<<"}\n";
 c.scopes.pop_back();
 return ok;
}
bool emitStatements(const std::vector<Statement>& statements,Context& c,
                    std::size_t depth,bool insideLoop){
 bool ok=true;
 for(const auto& statement:statements){
  const auto pad=indentation(depth);
  if(statement.kind==Statement::Kind::Let||statement.kind==Statement::Kind::Var){
   if(c.scopes.back().contains(statement.name)){
    c.errors.push_back("native C backend: duplicate variable "+statement.name);ok=false;continue;
   }
   if(insideLoop&&statement.kind==Statement::Kind::Let){
    c.errors.push_back("native C backend: use var for declarations inside while loops");ok=false;continue;
   }
   Type type;if(!infer(statement.expression,c,type)){ok=false;continue;}
   if(!statement.declaredType.empty()){
    const bool valid=(statement.declaredType=="number"&&type==Type::Number)||
                     (statement.declaredType=="string"&&type==Type::String)||
                     (statement.declaredType=="bool"&&type==Type::Boolean);
    if(!valid){c.errors.push_back("native C backend: annotation does not match initializer for "+statement.name);ok=false;continue;}
   }
   std::string expression;if(!emitExpr(statement.expression,c,expression)){ok=false;continue;}
   const auto symbol="taviq_v"+std::to_string(c.nextSymbol++);
   const bool immutable=statement.kind==Statement::Kind::Let;
   const std::string cType=type==Type::Number?"double":(type==Type::Boolean?"int":"const char*");
   c.code<<pad<<"  ";
   if(immutable&&type!=Type::String)c.code<<"const ";
   c.code<<cType;
   if(immutable&&type==Type::String)c.code<<" const";
   c.code<<" "<<symbol<<" = "<<expression<<";\n";
   c.scopes.back().emplace(statement.name,Binding{symbol,type,!immutable});
  }else if(statement.kind==Statement::Kind::Assign){
   const auto* binding=resolve(c,statement.name);
   if(!binding){c.errors.push_back("native C backend: assignment to undefined variable "+statement.name);ok=false;continue;}
   if(!binding->mutableLocal){c.errors.push_back("native C backend: cannot assign to immutable let "+statement.name);ok=false;continue;}
   Type type;if(!infer(statement.expression,c,type)||type!=binding->type){
    c.errors.push_back("native C backend: assignment type mismatch for "+statement.name);ok=false;continue;
   }
   std::string expression;if(!emitExpr(statement.expression,c,expression)){ok=false;continue;}
   c.code<<pad<<"  "<<binding->symbol<<" = "<<expression<<";\n";
  }else if(statement.kind==Statement::Kind::Display){
   Type type;if(!infer(statement.expression,c,type)){ok=false;continue;}
   std::string expression;if(!emitExpr(statement.expression,c,expression)){ok=false;continue;}
   if(type==Type::Number)c.code<<pad<<"  printf(\"%.15g\\n\",(double)("<<expression<<"));\n";
   else if(type==Type::Boolean)c.code<<pad<<"  puts(("<<expression<<") ? \"true\" : \"false\");\n";
   else c.code<<pad<<"  puts("<<expression<<");\n";
  }else if(statement.kind==Statement::Kind::If){
   Type type;if(!infer(statement.expression,c,type)||type!=Type::Boolean){
    c.errors.push_back("native C backend: if condition must be Boolean");ok=false;continue;
   }
   std::string condition;if(!emitExpr(statement.expression,c,condition)){ok=false;continue;}
   c.code<<pad<<"  if ("<<condition<<")\n";
   if(!emitBlock(statement.body,c,depth+1,insideLoop))ok=false;
   if(!statement.elseBody.empty()){
    c.code<<pad<<"  else\n";
    if(!emitBlock(statement.elseBody,c,depth+1,insideLoop))ok=false;
   }
  }else if(statement.kind==Statement::Kind::While){
   Type type;if(!infer(statement.expression,c,type)||type!=Type::Boolean){
    c.errors.push_back("native C backend: while condition must be Boolean");ok=false;continue;
   }
   std::string condition;if(!emitExpr(statement.expression,c,condition)){ok=false;continue;}
   c.code<<pad<<"  while ("<<condition<<")\n";
   if(!emitBlock(statement.body,c,depth+1,true))ok=false;
  }
 }
 return ok;
}
}
bool emitC(const Program& program,std::string& output,std::vector<std::string>& errors){
 Context context;
 if(!emitStatements(program.statements,context,1,false)||!context.errors.empty()){
  errors.insert(errors.end(),context.errors.begin(),context.errors.end());return false;
 }
 std::ostringstream result;
 result<<"#include <math.h>\n#include <stdio.h>\n#include <stdlib.h>\n#include <string.h>\n"
       <<"static double taviq_div(double a,double b){if(b==0.0){fputs(\"TAVIQ: division by zero\\\\n\",stderr);exit(1);}double v=a/b;if(!isfinite(v)){fputs(\"TAVIQ: non-finite result\\\\n\",stderr);exit(1);}return v;}\n"
       <<"static double taviq_sqrt(double a){if(a<0.0){fputs(\"TAVIQ: negative square root\\\\n\",stderr);exit(1);}double v=sqrt(a);if(!isfinite(v)){fputs(\"TAVIQ: non-finite result\\\\n\",stderr);exit(1);}return v;}\n"
       <<"int main(void){\n"<<context.code.str()<<"    return 0;\n}\n";
 output=result.str();return true;
}
}
