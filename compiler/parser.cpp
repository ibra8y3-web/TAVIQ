#include "taviq/parser.hpp"
namespace taviq {
const Token& Parser::peek()const{return tokens_[current_];}
const Token& Parser::advance(){if(!check(TokenKind::End))++current_;return tokens_[current_-1];}
bool Parser::check(TokenKind kind)const{return peek().kind==kind;}
bool Parser::match(TokenKind kind){if(!check(kind))return false;advance();return true;}
bool Parser::consume(TokenKind kind,const std::string& message){
 if(check(kind)){advance();return true;}
 errors_.push_back("Line "+std::to_string(peek().line)+": "+message);return false;
}
void Parser::synchronize(){
 while(!check(TokenKind::End)&&!check(TokenKind::RightBrace)){
  if(current_>0&&tokens_[current_-1].kind==TokenKind::Semicolon)return;
  if(check(TokenKind::Let)||check(TokenKind::Var)||check(TokenKind::Display)||
     check(TokenKind::If)||check(TokenKind::While)||check(TokenKind::Return)||check(TokenKind::Identifier))return;
  advance();
 }
}
Function Parser::functionDeclaration(){
 Function fn;fn.line=peek().line;advance();
 if(!check(TokenKind::Identifier)){
  errors_.push_back("Line "+std::to_string(peek().line)+": expected function name");synchronize();return fn;
 }
 fn.name=advance().lexeme;
 consume(TokenKind::LeftParen,"expected '(' after function name");
 if(!check(TokenKind::RightParen)){
  do{
   Parameter p;p.line=peek().line;
   if(!check(TokenKind::Identifier)){errors_.push_back("Line "+std::to_string(peek().line)+": expected parameter name");break;}
   p.name=advance().lexeme;
   if(consume(TokenKind::Colon,"expected ':' after parameter name")){
    if(check(TokenKind::Identifier))p.declaredType=advance().lexeme;
    else errors_.push_back("Line "+std::to_string(peek().line)+": expected parameter type");
   }
   fn.parameters.push_back(std::move(p));
  }while(match(TokenKind::Comma));
 }
 consume(TokenKind::RightParen,"expected ')' after parameters");
 consume(TokenKind::Arrow,"expected '->' before return type");
 if(check(TokenKind::Identifier))fn.returnType=advance().lexeme;
 else errors_.push_back("Line "+std::to_string(peek().line)+": expected function return type");
 consume(TokenKind::LeftBrace,"expected '{' before function body");
 fn.body=blockContents();
 return fn;
}
Statement Parser::declaration(TokenKind tokenKind){
 Statement s;s.kind=tokenKind==TokenKind::Var?Statement::Kind::Var:Statement::Kind::Let;s.line=peek().line;advance();
 if(!check(TokenKind::Identifier)){errors_.push_back("Line "+std::to_string(peek().line)+": expected variable name");s.expression=std::make_shared<Expr>();return s;}
 s.name=advance().lexeme;
 if(match(TokenKind::Colon)){
  if(!check(TokenKind::Identifier)){errors_.push_back("Line "+std::to_string(peek().line)+": expected type name after ':'");s.expression=std::make_shared<Expr>();return s;}
  s.declaredType=advance().lexeme;
 }
 if(!consume(TokenKind::Equal,"expected '=' after variable name or type annotation")){s.expression=std::make_shared<Expr>();return s;}
 s.expression=expression();consume(TokenKind::Semicolon,"expected ';' after declaration");return s;
}
Statement Parser::assignment(){
 Statement s;s.kind=Statement::Kind::Assign;s.line=peek().line;s.name=advance().lexeme;
 consume(TokenKind::Equal,"expected '=' after variable name");s.expression=expression();
 consume(TokenKind::Semicolon,"expected ';' after assignment");return s;
}
Statement Parser::displayStatement(){
 Statement s;s.kind=Statement::Kind::Display;s.line=peek().line;advance();s.expression=expression();
 consume(TokenKind::Semicolon,"expected ';' after display");return s;
}
Statement Parser::returnStatement(){
 Statement s;s.kind=Statement::Kind::Return;s.line=peek().line;advance();s.expression=expression();
 consume(TokenKind::Semicolon,"expected ';' after return value");return s;
}
std::vector<Statement> Parser::blockContents(){
 std::vector<Statement> out;
 while(!check(TokenKind::RightBrace)&&!check(TokenKind::End)){
  const auto before=current_;out.push_back(statement());
  if(current_==before&&!check(TokenKind::End))advance();
 }
 consume(TokenKind::RightBrace,"expected '}' to close block");return out;
}
Statement Parser::ifStatement(){
 Statement s;s.kind=Statement::Kind::If;s.line=peek().line;advance();
 consume(TokenKind::LeftParen,"expected '(' after if");s.expression=expression();
 consume(TokenKind::RightParen,"expected ')' after if condition");consume(TokenKind::LeftBrace,"expected '{' before if body");
 s.body=blockContents();
 if(match(TokenKind::Else)){consume(TokenKind::LeftBrace,"expected '{' before else body");s.elseBody=blockContents();}
 return s;
}
Statement Parser::whileStatement(){
 Statement s;s.kind=Statement::Kind::While;s.line=peek().line;advance();
 consume(TokenKind::LeftParen,"expected '(' after while");s.expression=expression();
 consume(TokenKind::RightParen,"expected ')' after while condition");consume(TokenKind::LeftBrace,"expected '{' before while body");
 s.body=blockContents();return s;
}
Statement Parser::statement(){
 if(check(TokenKind::Return))return returnStatement();
 if(check(TokenKind::Let))return declaration(TokenKind::Let);
 if(check(TokenKind::Var))return declaration(TokenKind::Var);
 if(check(TokenKind::Display))return displayStatement();
 if(check(TokenKind::If))return ifStatement();
 if(check(TokenKind::While))return whileStatement();
 if(check(TokenKind::Identifier)&&current_+1<tokens_.size()&&tokens_[current_+1].kind==TokenKind::Equal)return assignment();
 errors_.push_back("Line "+std::to_string(peek().line)+": expected declaration, assignment, display, return, if or while");
 synchronize();return {};
}
std::shared_ptr<Expr> Parser::expression(){return logicalOr();}
std::shared_ptr<Expr> Parser::logicalOr(){
 auto e=logicalAnd();while(match(TokenKind::Or)){auto n=std::make_shared<Expr>();n->kind=Expr::Kind::Binary;n->op='|';n->left=e;n->right=logicalAnd();e=n;}return e;
}
std::shared_ptr<Expr> Parser::logicalAnd(){
 auto e=equality();while(match(TokenKind::And)){auto n=std::make_shared<Expr>();n->kind=Expr::Kind::Binary;n->op='&';n->left=e;n->right=equality();e=n;}return e;
}
std::shared_ptr<Expr> Parser::equality(){
 auto e=comparison();while(match(TokenKind::EqualEqual)||match(TokenKind::BangEqual)){
  const auto k=tokens_[current_-1].kind;auto n=std::make_shared<Expr>();n->kind=Expr::Kind::Binary;
  n->op=k==TokenKind::EqualEqual?'q':'!';n->left=e;n->right=comparison();e=n;}return e;
}
std::shared_ptr<Expr> Parser::comparison(){
 auto e=term();while(match(TokenKind::Less)||match(TokenKind::LessEqual)||match(TokenKind::Greater)||match(TokenKind::GreaterEqual)){
  const auto k=tokens_[current_-1].kind;char op='<';if(k==TokenKind::LessEqual)op='l';else if(k==TokenKind::Greater)op='>';else if(k==TokenKind::GreaterEqual)op='g';
  auto n=std::make_shared<Expr>();n->kind=Expr::Kind::Binary;n->op=op;n->left=e;n->right=term();e=n;}return e;
}
std::shared_ptr<Expr> Parser::term(){
 auto e=factor();while(match(TokenKind::Plus)||match(TokenKind::Minus)){char op=tokens_[current_-1].kind==TokenKind::Plus?'+':'-';
  auto n=std::make_shared<Expr>();n->kind=Expr::Kind::Binary;n->op=op;n->left=e;n->right=factor();e=n;}return e;
}
std::shared_ptr<Expr> Parser::factor(){
 auto e=unary();while(match(TokenKind::Star)||match(TokenKind::Slash)){char op=tokens_[current_-1].kind==TokenKind::Star?'*':'/';
  auto n=std::make_shared<Expr>();n->kind=Expr::Kind::Binary;n->op=op;n->left=e;n->right=unary();e=n;}return e;
}
std::shared_ptr<Expr> Parser::unary(){
 if(match(TokenKind::Minus)||match(TokenKind::Not)||match(TokenKind::Bang)){const auto k=tokens_[current_-1].kind;
  auto n=std::make_shared<Expr>();n->kind=Expr::Kind::Unary;n->op=k==TokenKind::Minus?'-':'!';n->right=unary();return n;}return primary();
}
std::shared_ptr<Expr> Parser::primary(){
 if(match(TokenKind::Number)){auto n=std::make_shared<Expr>();n->kind=Expr::Kind::Number;n->text=tokens_[current_-1].lexeme;return n;}
 if(match(TokenKind::String)){auto n=std::make_shared<Expr>();n->kind=Expr::Kind::String;n->text=tokens_[current_-1].lexeme;return n;}
 if(match(TokenKind::True)||match(TokenKind::False)){auto n=std::make_shared<Expr>();n->kind=Expr::Kind::Boolean;n->boolean=tokens_[current_-1].kind==TokenKind::True;return n;}
 if(match(TokenKind::Identifier)){
  const auto name=tokens_[current_-1].lexeme;
  if(match(TokenKind::LeftParen)){
   auto call=std::make_shared<Expr>();call->kind=Expr::Kind::Call;call->text=name;
   if(!check(TokenKind::RightParen)){do{call->arguments.push_back(expression());}while(match(TokenKind::Comma));}
   consume(TokenKind::RightParen,"expected ')' after function arguments");return call;
  }
  auto n=std::make_shared<Expr>();n->kind=Expr::Kind::Variable;n->text=name;return n;
 }
 if(match(TokenKind::LeftParen)){auto n=expression();consume(TokenKind::RightParen,"expected ')' after expression");return n;}
 errors_.push_back("Line "+std::to_string(peek().line)+": expected a value, variable, call or '('");
 auto fallback=std::make_shared<Expr>();fallback->kind=Expr::Kind::Boolean;fallback->boolean=false;
 if(!check(TokenKind::End)&&!check(TokenKind::RightBrace)&&!check(TokenKind::Semicolon))advance();
 return fallback;
}
Program Parser::parse(){
 Program p;while(check(TokenKind::Fn))p.functions.push_back(functionDeclaration());
 if(!consume(TokenKind::Flow,"expected 'flow'")||!consume(TokenKind::Main,"expected 'main' after 'flow'")||
    !consume(TokenKind::LeftBrace,"expected '{' after flow main"))return p;
 p.statements=blockContents();
 if(!check(TokenKind::End))errors_.push_back("Line "+std::to_string(peek().line)+": unexpected content after main flow");
 return p;
}
}
