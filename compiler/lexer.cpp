#include "taviq/lexer.hpp"
#include <cctype>
#include <utility>
namespace taviq {
Lexer::Lexer(std::string source):source_(std::move(source)){}
char Lexer::peek(std::size_t offset)const{return current_+offset<source_.size()?source_[current_+offset]:'\0';}
char Lexer::advance(){return current_<source_.size()?source_[current_++]:'\0';}
bool Lexer::match(char expected){if(peek()!=expected)return false;++current_;return true;}
void Lexer::add(TokenKind kind,std::string text){tokens_.push_back(Token{kind,std::move(text),line_});}
void Lexer::skipWhitespace(){
 for(;;){const char c=peek();if(c==' '||c=='\r'||c=='\t')advance();
 else if(c=='\n'){advance();++line_;}
 else if(c=='/'&&peek(1)=='/'){while(peek()!='\n'&&peek()!='\0')advance();}
 else break;}
}
void Lexer::scanIdentifier(){
 const auto start=current_-1;while(std::isalnum(static_cast<unsigned char>(peek()))||peek()=='_')advance();
 auto word=source_.substr(start,current_-start);
 if(word=="flow")add(TokenKind::Flow,word);else if(word=="main")add(TokenKind::Main,word);
 else if(word=="let")add(TokenKind::Let,word);else if(word=="var")add(TokenKind::Var,word);
 else if(word=="display")add(TokenKind::Display,word);else if(word=="if")add(TokenKind::If,word);
 else if(word=="else")add(TokenKind::Else,word);else if(word=="while")add(TokenKind::While,word);
 else if(word=="true")add(TokenKind::True,word);else if(word=="false")add(TokenKind::False,word);
 else if(word=="and")add(TokenKind::And,word);else if(word=="or")add(TokenKind::Or,word);
 else if(word=="not")add(TokenKind::Not,word);else add(TokenKind::Identifier,std::move(word));
}
void Lexer::scanNumber(){
 const auto start=current_-1;while(std::isdigit(static_cast<unsigned char>(peek())))advance();
 if(peek()=='.'&&std::isdigit(static_cast<unsigned char>(peek(1)))){advance();while(std::isdigit(static_cast<unsigned char>(peek())))advance();}
 if(peek()=='e'||peek()=='E'){const auto save=current_;advance();if(peek()=='+'||peek()=='-')advance();
  if(!std::isdigit(static_cast<unsigned char>(peek())))current_=save;else while(std::isdigit(static_cast<unsigned char>(peek())))advance();}
 add(TokenKind::Number,source_.substr(start,current_-start));
}
void Lexer::scanString(){
 std::string value;while(peek()!='"'&&peek()!='\0'){const char c=advance();if(c=='\n')++line_;
  if(c=='\\'){const char e=advance();if(e=='\0')break;switch(e){
   case 'n':value.push_back('\n');break;case 't':value.push_back('\t');break;case 'r':value.push_back('\r');break;
   case '"':value.push_back('"');break;case '\\':value.push_back('\\');break;
   default:errors_.push_back("Line "+std::to_string(line_)+": unsupported escape sequence");value.push_back(e);}}
  else value.push_back(c);}
 if(peek()=='\0'){errors_.push_back("Line "+std::to_string(line_)+": unterminated string");return;}
 advance();add(TokenKind::String,std::move(value));
}
std::vector<Token> Lexer::scan(){
 while(current_<source_.size()){
  skipWhitespace();if(current_>=source_.size())break;const char c=advance();
  switch(c){
   case '{':add(TokenKind::LeftBrace,"{");break;case '}':add(TokenKind::RightBrace,"}");break;
   case '(':add(TokenKind::LeftParen,"(");break;case ')':add(TokenKind::RightParen,")");break;
   case ';':add(TokenKind::Semicolon,";");break;case ',':add(TokenKind::Comma,",");break;
   case '=':{const bool eq=match('=');add(eq?TokenKind::EqualEqual:TokenKind::Equal,eq?"==":"=");break;}
   case '!':{const bool eq=match('=');add(eq?TokenKind::BangEqual:TokenKind::Bang,eq?"!=":"!");break;}
   case '<':{const bool eq=match('=');add(eq?TokenKind::LessEqual:TokenKind::Less,eq?"<=":"<");break;}
   case '>':{const bool eq=match('=');add(eq?TokenKind::GreaterEqual:TokenKind::Greater,eq?">=":">");break;}
   case '+':add(TokenKind::Plus,"+");break;case '-':add(TokenKind::Minus,"-");break;
   case '*':add(TokenKind::Star,"*");break;case '/':add(TokenKind::Slash,"/");break;
   case '"':scanString();break;
   default:if(std::isalpha(static_cast<unsigned char>(c))||c=='_')scanIdentifier();
    else if(std::isdigit(static_cast<unsigned char>(c)))scanNumber();
    else errors_.push_back("Line "+std::to_string(line_)+": unexpected character '"+std::string(1,c)+"'");
  }
 }
 tokens_.push_back(Token{TokenKind::End,"",line_});return tokens_;
}
}
