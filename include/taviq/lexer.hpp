#pragma once
#include "taviq/token.hpp"
#include <string>
#include <vector>
namespace taviq {
class Lexer {
public:
    explicit Lexer(std::string source);
    std::vector<Token> scan();
    const std::vector<std::string>& errors() const { return errors_; }
private:
    char peek(std::size_t offset=0) const; char advance(); bool match(char expected);
    void skipWhitespace(); void scanIdentifier(); void scanNumber(); void scanString();
    void add(TokenKind kind, std::string text);
    std::string source_; std::size_t current_{0}, line_{1};
    std::vector<Token> tokens_; std::vector<std::string> errors_;
};
}