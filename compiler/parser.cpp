#include "taviq/parser.hpp"
namespace taviq {
const Token& Parser::peek()const{return tokens_[current_];}
const Token& Parser::advance(){if(!check(TokenKind::End))++current_;return tokens_[current_-1];}
bool Parser::check(TokenKind kind)const{return peek().kind==kind;}
bool Parser::match(TokenKind kind){if(!check(kind))return false;advance();return true;}
bool Parser::consume(TokenKind kind,const std::string& message){
 if(check(kind)){advance();return true;}errors_.push_back("Line "+std::to_string(peek().line)+": "+message);return false;
}
void Parser::synchronize(){while(!check(TokenKind::End)&&!check(TokenKind::Display)&&!check(TokenKind::Let)&&!check(TokenKind::RightBrace))advance();}
Statement Parser::declaration(){
 Statement s;s.kind=Statement::Kind::Let;s.line=peek().line;advance();
 if(!check(TokenKind::Identifier)){errors_.push_back("Line "+std::to_string(peek().line)+": expected variable name");s.expression=std::make_shared<Expr>();return s;}
 s.name=advance().lexeme;
 if(!consume(TokenKind::Equal,"expected '=' after variable name")){s.expression=std::make_shared<Expr>();return s;}
 s.expression=expression();consume(TokenKind::Semicolon,"expected ';' after declaration");return s;
}
Statement Parser::displayStatement(){
 Statement s;s.kind=Statement::Kind::Display;s.line=peek().line;advance();
 s.expression=expression();consume(TokenKind::Semicolon,"expected ';' after display");return s;
}
std::shared_ptr<Expr> Parser::expression(){return term();}
std::shared_ptr<Expr> Parser::term(){
 auto e=factor();
 while(match(TokenKind::Plus)||match(TokenKind::Minus)){
  char op=tokens_[current_-1].kind==TokenKind::Plus?'+':'-';auto r=factor();
  auto n=std::make_shared<Expr>();n->kind=Expr::Kind::Binary;n->op=op;n->left=e;n->right=r;e=n;
 }return e;
}
std::shared_ptr<Expr> Parser::factor(){
 auto e=unary();
 while(match(TokenKind::Star)||match(TokenKind::Slash)){
  char op=tokens_[current_-1].kind==TokenKind::Star?'*':'/';auto r=unary();
  auto n=std::make_shared<Expr>();n->kind=Expr::Kind::Binary;n->op=op;n->left=e;n->right=r;e=n;
 }return e;
}
std::shared_ptr<Expr> Parser::unary(){
 if(match(TokenKind::Minus)){auto n=std::make_shared<Expr>();n->kind=Expr::Kind::Unary;n->op='-';n->right=unary();return n;}
 return primary();
}
std::shared_ptr<Expr> Parser::primary(){
 if(match(TokenKind::Number)){auto e=std::make_shared<Expr>();e->kind=Expr::Kind::Number;e->text=tokens_[current_-1].lexeme;return e;}
 if(match(TokenKind::String)){auto e=std::make_shared<Expr>();e->kind=Expr::Kind::String;e->text=tokens_[current_-1].lexeme;return e;}
 if(match(TokenKind::Identifier)){auto e=std::make_shared<Expr>();e->kind=Expr::Kind::Variable;e->text=tokens_[current_-1].lexeme;return e;}
 if(match(TokenKind::LeftParen)){auto e=expression();consume(TokenKind::RightParen,"expected ')' after expression");return e;}
 errors_.push_back("Line "+std::to_string(peek().line)+": expected a number, string, variable, or '('");
 auto e=std::make_shared<Expr>();e->kind=Expr::Kind::Number;e->text="0";
 if(!check(TokenKind::End)&&!check(TokenKind::RightBrace)&&!check(TokenKind::Semicolon))advance();return e;
}
Program Parser::parse(){
 Program p;
 if(!consume(TokenKind::Flow,"expected 'flow'")||!consume(TokenKind::Main,"expected 'main' after 'flow'")||!consume(TokenKind::LeftBrace,"expected '{' after flow main"))return p;
 while(!check(TokenKind::RightBrace)&&!check(TokenKind::End)){
  const auto before=current_;
  if(check(TokenKind::Let))p.statements.push_back(declaration());
  else if(check(TokenKind::Display))p.statements.push_back(displayStatement());
  else{errors_.push_back("Line "+std::to_string(peek().line)+": expected 'let' or 'display'");synchronize();}
  if(current_==before&&!check(TokenKind::End))advance();
 }
 consume(TokenKind::RightBrace,"expected '}' to close flow main");
 if(!check(TokenKind::End))errors_.push_back("Line "+std::to_string(peek().line)+": unexpected content after main flow");
 return p;
}
}