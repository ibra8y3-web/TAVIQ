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
    enum class Kind { Let, Var, Assign, Display, If, While, Return };
    Kind kind{Kind::Display};
    std::string name;
    std::string declaredType; // number, string or bool; empty means infer
    std::shared_ptr<Expr> expression;
    std::size_t line{};
    std::vector<Statement> body;
    std::vector<Statement> elseBody;
};
struct Parameter { std::string name; std::string declaredType; std::size_t line{}; };
struct Function { std::string name; std::vector<Parameter> parameters; std::string returnType; std::size_t line{}; std::vector<Statement> body; };
struct Program { std::vector<Function> functions; std::vector<Statement> statements; };
}
