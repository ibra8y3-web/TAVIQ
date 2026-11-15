#include "taviq/parser.hpp"
namespace taviq {
const Token& Parser::peek()const{return tokens_[current_];}
const Token& Parser::advance(){if(!check(TokenKind::End))++current_;return tokens_[current_-1];}
bool Parser::check(TokenKind kind)const{return peek().kind==kind;}
bool Parser::match(TokenKind kind){if(!check(kind))return false;advance();return true;}
bool Parser::consume(TokenKind kind,const std::string& message){
    if(check(kind)){advance();return true;}
    errors_.push_back("Line "+std::to_string(peek().line)+": "+message);
    return false;
}
void Parser::synchronize(){
    while(!check(TokenKind::End)&&!check(TokenKind::RightBrace)){
        if(current_>0&&tokens_[current_-1].kind==TokenKind::Semicolon)return;
        if(check(TokenKind::Let)||check(TokenKind::Var)||check(TokenKind::Display)||
           check(TokenKind::If)||check(TokenKind::While)||check(TokenKind::Identifier))return;
        advance();
    }
}
Statement Parser::declaration(TokenKind tokenKind){
    Statement result;
    result.kind=tokenKind==TokenKind::Var?Statement::Kind::Var:Statement::Kind::Let;
    result.line=peek().line;
    advance();
    if(!check(TokenKind::Identifier)){
        errors_.push_back("Line "+std::to_string(peek().line)+": expected variable name");
        result.expression=std::make_shared<Expr>();
        return result;
    }
    result.name=advance().lexeme;
    if(!consume(TokenKind::Equal,"expected '=' after variable name")){
        result.expression=std::make_shared<Expr>();
        return result;
    }
    result.expression=expression();
    consume(TokenKind::Semicolon,"expected ';' after declaration");
    return result;
}
Statement Parser::assignment(){
    Statement result;
    result.kind=Statement::Kind::Assign;
    result.line=peek().line;
    result.name=advance().lexeme;
    consume(TokenKind::Equal,"expected '=' after variable name");
    result.expression=expression();
    consume(TokenKind::Semicolon,"expected ';' after assignment");
    return result;
}
Statement Parser::displayStatement(){
    Statement result;
    result.kind=Statement::Kind::Display;
    result.line=peek().line;
    advance();
    result.expression=expression();
    consume(TokenKind::Semicolon,"expected ';' after display");
    return result;
}
std::vector<Statement> Parser::blockContents(){
    std::vector<Statement> result;
    while(!check(TokenKind::RightBrace)&&!check(TokenKind::End)){
        const auto before=current_;
        result.push_back(statement());
        if(current_==before&&!check(TokenKind::End))advance();
    }
    consume(TokenKind::RightBrace,"expected '}' to close block");
    return result;
}
Statement Parser::ifStatement(){
    Statement result;
    result.kind=Statement::Kind::If;
    result.line=peek().line;
    advance();
    consume(TokenKind::LeftParen,"expected '(' after if");
    result.expression=expression();
    consume(TokenKind::RightParen,"expected ')' after if condition");
    consume(TokenKind::LeftBrace,"expected '{' before if body");
    result.body=blockContents();
    if(match(TokenKind::Else)){
        consume(TokenKind::LeftBrace,"expected '{' before else body");
        result.elseBody=blockContents();
    }
    return result;
}
Statement Parser::whileStatement(){
    Statement result;
    result.kind=Statement::Kind::While;
    result.line=peek().line;
    advance();
    consume(TokenKind::LeftParen,"expected '(' after while");
    result.expression=expression();
    consume(TokenKind::RightParen,"expected ')' after while condition");
    consume(TokenKind::LeftBrace,"expected '{' before while body");
    result.body=blockContents();
    return result;
}
Statement Parser::statement(){
    if(check(TokenKind::Let))return declaration(TokenKind::Let);
    if(check(TokenKind::Var))return declaration(TokenKind::Var);
    if(check(TokenKind::Display))return displayStatement();
    if(check(TokenKind::If))return ifStatement();
    if(check(TokenKind::While))return whileStatement();
    if(check(TokenKind::Identifier)&&current_+1<tokens_.size()&&tokens_[current_+1].kind==TokenKind::Equal)
        return assignment();
    errors_.push_back("Line "+std::to_string(peek().line)+": expected let, var, assignment, display, if or while");
    synchronize();
    return {};
}
std::shared_ptr<Expr> Parser::expression(){return logicalOr();}
std::shared_ptr<Expr> Parser::logicalOr(){
    auto expr=logicalAnd();
    while(match(TokenKind::Or)){
        auto node=std::make_shared<Expr>();node->kind=Expr::Kind::Binary;node->op='|';node->left=expr;node->right=logicalAnd();expr=node;
    }
    return expr;
}
std::shared_ptr<Expr> Parser::logicalAnd(){
    auto expr=equality();
    while(match(TokenKind::And)){
        auto node=std::make_shared<Expr>();node->kind=Expr::Kind::Binary;node->op='&';node->left=expr;node->right=equality();expr=node;
    }
    return expr;
}
std::shared_ptr<Expr> Parser::equality(){
    auto expr=comparison();
    while(match(TokenKind::EqualEqual)||match(TokenKind::BangEqual)){
        const auto kind=tokens_[current_-1].kind;
        auto node=std::make_shared<Expr>();node->kind=Expr::Kind::Binary;node->op=kind==TokenKind::EqualEqual?'q':'!';
        node->left=expr;node->right=comparison();expr=node;
    }
    return expr;
}
std::shared_ptr<Expr> Parser::comparison(){
    auto expr=term();
    while(match(TokenKind::Less)||match(TokenKind::LessEqual)||match(TokenKind::Greater)||match(TokenKind::GreaterEqual)){
        const auto kind=tokens_[current_-1].kind;
        char op='<';
        if(kind==TokenKind::LessEqual)op='l';else if(kind==TokenKind::Greater)op='>';else if(kind==TokenKind::GreaterEqual)op='g';
        auto node=std::make_shared<Expr>();node->kind=Expr::Kind::Binary;node->op=op;node->left=expr;node->right=term();expr=node;
    }
    return expr;
}
std::shared_ptr<Expr> Parser::term(){
    auto expr=factor();
    while(match(TokenKind::Plus)||match(TokenKind::Minus)){
        const char op=tokens_[current_-1].kind==TokenKind::Plus?'+':'-';
        auto node=std::make_shared<Expr>();node->kind=Expr::Kind::Binary;node->op=op;node->left=expr;node->right=factor();expr=node;
    }
    return expr;
}
std::shared_ptr<Expr> Parser::factor(){
    auto expr=unary();
    while(match(TokenKind::Star)||match(TokenKind::Slash)){
        const char op=tokens_[current_-1].kind==TokenKind::Star?'*':'/';
        auto node=std::make_shared<Expr>();node->kind=Expr::Kind::Binary;node->op=op;node->left=expr;node->right=unary();expr=node;
    }
    return expr;
}
std::shared_ptr<Expr> Parser::unary(){
    if(match(TokenKind::Minus)||match(TokenKind::Not)||match(TokenKind::Bang)){
        const auto kind=tokens_[current_-1].kind;
        auto node=std::make_shared<Expr>();node->kind=Expr::Kind::Unary;node->op=kind==TokenKind::Minus?'-':'!';node->right=unary();return node;
    }
    return primary();
}
std::shared_ptr<Expr> Parser::primary(){
    if(match(TokenKind::Number)){auto node=std::make_shared<Expr>();node->kind=Expr::Kind::Number;node->text=tokens_[current_-1].lexeme;return node;}
    if(match(TokenKind::String)){auto node=std::make_shared<Expr>();node->kind=Expr::Kind::String;node->text=tokens_[current_-1].lexeme;return node;}
    if(match(TokenKind::True)||match(TokenKind::False)){
        auto node=std::make_shared<Expr>();node->kind=Expr::Kind::Boolean;node->boolean=tokens_[current_-1].kind==TokenKind::True;return node;
    }
    if(match(TokenKind::Identifier)){
        const auto identifier=tokens_[current_-1].lexeme;
        if(match(TokenKind::LeftParen)){
            auto call=std::make_shared<Expr>();call->kind=Expr::Kind::Call;call->text=identifier;
            if(!check(TokenKind::RightParen)){do{call->arguments.push_back(expression());}while(match(TokenKind::Comma));}
            consume(TokenKind::RightParen,"expected ')' after function arguments");
            return call;
        }
        auto node=std::make_shared<Expr>();node->kind=Expr::Kind::Variable;node->text=identifier;return node;
    }
    if(match(TokenKind::LeftParen)){
        auto node=expression();consume(TokenKind::RightParen,"expected ')' after expression");return node;
    }
    errors_.push_back("Line "+std::to_string(peek().line)+": expected a value, variable, call or '('");
    auto fallback=std::make_shared<Expr>();fallback->kind=Expr::Kind::Boolean;fallback->boolean=false;
    if(!check(TokenKind::End)&&!check(TokenKind::RightBrace)&&!check(TokenKind::Semicolon))advance();
    return fallback;
}
Program Parser::parse(){
    Program program;
    if(!consume(TokenKind::Flow,"expected 'flow'")||
       !consume(TokenKind::Main,"expected 'main' after 'flow'")||
       !consume(TokenKind::LeftBrace,"expected '{' after flow main"))return program;
    program.statements=blockContents();
    if(!check(TokenKind::End))errors_.push_back("Line "+std::to_string(peek().line)+": unexpected content after main flow");
    return program;
}
}
