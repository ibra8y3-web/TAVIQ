#pragma once
#include <cstddef>
#include <memory>
#include <string>
#include <vector>
namespace taviq {
struct Expr {
    enum class Kind { Number, String, Variable, Binary, Unary };
    Kind kind{Kind::Number}; double number{}; std::string text; char op{};
    std::shared_ptr<Expr> left, right;
};
struct Statement {
    enum class Kind { Let, Display };
    Kind kind{Kind::Display}; std::string name; std::shared_ptr<Expr> expression; std::size_t line{};
};
struct Program { std::vector<Statement> statements; };
}