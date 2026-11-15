#pragma once
#include "taviq/ast.hpp"
#include "taviq/token.hpp"
#include <string>
#include <vector>
namespace taviq {
class Parser {
public:
    explicit Parser(const std::vector<Token>& tokens) : tokens_(tokens) {}
    Program parse();
    const std::vector<std::string>& errors() const { return errors_; }
private:
    const Token& peek() const; const Token& advance(); bool check(TokenKind kind) const;
    bool match(TokenKind kind); bool consume(TokenKind kind, const std::string& message);
    void synchronize(); Statement declaration(); Statement displayStatement();
    std::shared_ptr<Expr> expression(); std::shared_ptr<Expr> term();
    std::shared_ptr<Expr> factor(); std::shared_ptr<Expr> unary(); std::shared_ptr<Expr> primary();
    const std::vector<Token>& tokens_; std::size_t current_{0}; std::vector<std::string> errors_;
};
}