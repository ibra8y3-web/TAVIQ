#include "taviq/lexer.hpp"
#include <cctype>
#include <utility>

namespace taviq {
Lexer::Lexer(std::string source) : source_(std::move(source)) {}
char Lexer::peek() const { return current_ < source_.size() ? source_[current_] : '\0'; }
char Lexer::advance() { return current_ < source_.size() ? source_[current_++] : '\0'; }
bool Lexer::match(char expected) {
    if (peek() != expected) return false;
    ++current_; return true;
}
void Lexer::add(TokenKind kind, std::string text) {
    tokens_.push_back(Token{kind, std::move(text), line_});
}
void Lexer::skipWhitespace() {
    while (true) {
        char c = peek();
        if (c == ' ' || c == '\r' || c == '\t') advance();
        else if (c == '\n') { advance(); ++line_; }
        else if (c == '/' && current_ + 1 < source_.size() && source_[current_+1] == '/') {
            while (peek() != '\n' && peek() != '\0') advance();
        } else break;
    }
}
void Lexer::scanIdentifier() {
    const auto start = current_ - 1;
    while (std::isalnum(static_cast<unsigned char>(peek())) || peek() == '_') advance();
    auto word = source_.substr(start, current_ - start);
    if (word == "flow") add(TokenKind::Flow, word);
    else if (word == "main") add(TokenKind::Main, word);
    else if (word == "display") add(TokenKind::Display, word);
    else errors_.push_back("Line " + std::to_string(line_) + ": unknown keyword '" + word + "'");
}
void Lexer::scanString() {
    std::string value;
    while (peek() != '"' && peek() != '\0') {
        char c = advance();
        if (c == '\n') ++line_;
        if (c == '\\' && peek() != '\0') {
            char escaped = advance();
            if (escaped == 'n') value.push_back('\n');
            else if (escaped == 't') value.push_back('\t');
            else value.push_back(escaped);
        } else value.push_back(c);
    }
    if (peek() == '\0') {
        errors_.push_back("Line " + std::to_string(line_) + ": unterminated string");
        return;
    }
    advance();
    add(TokenKind::String, value);
}
std::vector<Token> Lexer::scan() {
    while (current_ < source_.size()) {
        skipWhitespace();
        if (current_ >= source_.size()) break;
        char c = advance();
        switch (c) {
            case '{': add(TokenKind::LeftBrace, "{"); break;
            case '}': add(TokenKind::RightBrace, "}"); break;
            case '(': add(TokenKind::LeftParen, "("); break;
            case ')': add(TokenKind::RightParen, ")"); break;
            case ';': add(TokenKind::Semicolon, ";"); break;
            case '"': scanString(); break;
            default:
                if (std::isalpha(static_cast<unsigned char>(c)) || c == '_') scanIdentifier();
                else errors_.push_back("Line " + std::to_string(line_) + ": unexpected character");
        }
    }
    tokens_.push_back(Token{TokenKind::End, "", line_});
    return tokens_;
}
}
