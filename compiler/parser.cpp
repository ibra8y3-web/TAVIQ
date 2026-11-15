#include "taviq/parser.hpp"

namespace taviq {
const Token& Parser::peek() const { return tokens_[current_]; }
const Token& Parser::advance() { if (!check(TokenKind::End)) ++current_; return tokens_[current_-1]; }
bool Parser::check(TokenKind kind) const { return peek().kind == kind; }
bool Parser::consume(TokenKind kind, const std::string& message) {
    if (check(kind)) { advance(); return true; }
    errors_.push_back("Line " + std::to_string(peek().line) + ": " + message);
    return false;
}
void Parser::synchronize() {
    while (!check(TokenKind::End) && !check(TokenKind::Display) && !check(TokenKind::RightBrace)) advance();
}
Program Parser::parse() {
    Program program;
    if (!consume(TokenKind::Flow, "expected 'flow'")) return program;
    if (!consume(TokenKind::Main, "expected 'main' after 'flow'")) return program;
    if (!consume(TokenKind::LeftBrace, "expected '{' after flow main")) return program;

    while (!check(TokenKind::RightBrace) && !check(TokenKind::End)) {
        if (!consume(TokenKind::Display, "expected 'display' statement")) {
            synchronize(); continue;
        }
        if (!check(TokenKind::String)) {
            errors_.push_back("Line " + std::to_string(peek().line) + ": display expects a string literal");
            synchronize(); continue;
        }
        auto value = advance();
        if (!consume(TokenKind::Semicolon, "expected ';' after display")) {
            synchronize(); continue;
        }
        program.statements.push_back(Statement{value.lexeme, value.line});
    }
    consume(TokenKind::RightBrace, "expected '}' to close flow main");
    if (!check(TokenKind::End))
        errors_.push_back("Line " + std::to_string(peek().line) + ": unexpected content after main flow");
    return program;
}
}
