#pragma once
#include <cstddef>
#include <memory>
#include <string>
#include <vector>
namespace taviq {
struct Expr {
    enum class Kind { Number, String, Boolean, Variable, Binary, Unary, Call };
    Kind kind{Kind::Number};
    double number{};
    bool boolean{};
    std::string text;
    char op{};
    std::shared_ptr<Expr> left, right;
    std::vector<std::shared_ptr<Expr>> arguments;
};
struct Statement {
    enum class Kind { Let, Var, Assign, Display, If, While };
    Kind kind{Kind::Display};
    std::string name;
    std::shared_ptr<Expr> expression;
    std::size_t line{};
    std::vector<Statement> body;
    std::vector<Statement> elseBody;
};
struct Program { std::vector<Statement> statements; };
}
