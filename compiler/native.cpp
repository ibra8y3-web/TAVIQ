#include "taviq/native.hpp"
#include <cstdlib>
#include <iomanip>
#include <sstream>
#include <unordered_map>
namespace taviq {
namespace {
enum class Type{Number,String};
bool getType(const std::shared_ptr<Expr>& e,const std::unordered_map<std::string,Type>& vars,Type& t,std::vector<std::string>& errors){
 if(!e){errors.push_back("missing expression");return false;}if(e->kind==Expr::Kind::Number){t=Type::Number;return true;}if(e->kind==Expr::Kind::String){t=Type::String;return true;}if(e->kind==Expr::Kind::Variable){auto i=vars.find(e->text);if(i==vars.end()){errors.push_back("undefined variable: "+e->text);return false;}t=i->second;return true;}
 if(e->kind==Expr::Kind::Call){if(e->arguments.size()!=1){errors.push_back("native C backend: built-in requires one argument");return false;}Type a;if(!getType(e->arguments.front(),vars,a,errors))return false;if(e->text=="len"&&a==Type::String){t=Type::Number;return true;}if((e->text=="sqrt"||e->text=="abs")&&a==Type::Number){t=Type::Number;return true;}errors.push_back("native C backend: unsupported built-in or argument type: "+e->text);return false;}
 if(e->kind==Expr::Kind::Unary){Type a;if(!getType(e->right,vars,a,errors))return false;if(a!=Type::Number){errors.push_back("native C backend: unary '-' requires number");return false;}t=Type::Number;return true;}Type a,b;if(!getType(e->left,vars,a,errors)||!getType(e->right,vars,b,errors))return false;if(a==Type::String&&b==Type::String&&e->op=='+'){errors.push_back("native C backend currently does not lower string concatenation");return false;}if(a!=Type::Number||b!=Type::Number){errors.push_back("native C backend: arithmetic requires numbers");return false;}t=Type::Number;return true;
}
std::string quoteC(const std::string& s){std::ostringstream o;o<<'"';for(unsigned char c:s){switch(c){case '\\':o<<"\\\\";break;case '"':o<<"\\\"";break;case '\n':o<<"\\n";break;case '\r':o<<"\\r";break;case '\t':o<<"\\t";break;default:if(c<32||c>=127){o<<"\\"<<std::oct<<std::setw(3)<<std::setfill('0')<<static_cast<int>(c)<<std::dec;}else o<<static_cast<char>(c);}}o<<'"';return o.str();}
bool numeric(const std::shared_ptr<Expr>& e,const std::unordered_map<std::string,std::string>& vars,std::string& out,std::vector<std::string>& errors){
 if(e->kind==Expr::Kind::Number){char* end=nullptr;std::strtod(e->text.c_str(),&end);if(!end||*end!='\0'){errors.push_back("invalid number literal");return false;}out=e->text;return true;}if(e->kind==Expr::Kind::Variable){auto i=vars.find(e->text);if(i==vars.end()){errors.push_back("undefined variable: "+e->text);return false;}out=i->second;return true;}if(e->kind==Expr::Kind::Unary){std::string r;if(!numeric(e->right,vars,r,errors))return false;out="(-("+r+"))";return true;}if(e->kind==Expr::Kind::Binary){std::string l,r;if(!numeric(e->left,vars,l,errors)||!numeric(e->right,vars,r,errors))return false;out="(("+l+") "+e->op+" ("+r+"))";return true;}
 if(e->kind==Expr::Kind::Call&&e->arguments.size()==1){if(e->text=="len"&&e->arguments[0]->kind==Expr::Kind::String){out="((double)strlen("+quoteC(e->arguments[0]->text)+"))";return true;}std::string arg;if(!numeric(e->arguments[0],vars,arg,errors))return false;if(e->text=="sqrt"){out="sqrt("+arg+")";return true;}if(e->text=="abs"){out="fabs("+arg+")";return true;}}
 errors.push_back("native C backend: expression is not supported by native lowering");return false;
}
}
bool emitC(const Program& p,std::string& output,std::vector<std::string>& errors){
 std::ostringstream c;c<<"#include <stdio.h>\n#include <math.h>\n#include <string.h>\nint main(void) {\n";std::unordered_map<std::string,std::string> vars;std::unordered_map<std::string,Type> types;std::size_t next=0;
 for(const auto& s:p.statements){if(s.kind==Statement::Kind::Let){if(vars.contains(s.name)){errors.push_back("duplicate variable: "+s.name);continue;}Type t;if(!getType(s.expression,types,t,errors))continue;if(t!=Type::Number){errors.push_back("native C backend: let variables must be numeric");continue;}std::string expr;if(!numeric(s.expression,vars,expr,errors))continue;std::string name="taviq_v"+std::to_string(next++);c<<"  double "<<name<<" = "<<expr<<";\n";vars.emplace(s.name,name);types.emplace(s.name,Type::Number);}else if(s.expression&&s.expression->kind==Expr::Kind::String)c<<"  puts("<<quoteC(s.expression->text)<<");\n";else{Type t;if(!getType(s.expression,types,t,errors))continue;if(t!=Type::Number){errors.push_back("native C backend: display expects a string literal or number");continue;}std::string expr;if(numeric(s.expression,vars,expr,errors))c<<"  printf(\"%.15g\\n\", (double)("<<expr<<"));\n";}}
 if(!errors.empty())return false;c<<"  return 0;\n}\n";output=c.str();return true;
}
}