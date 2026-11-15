#include "taviq/ast.hpp"
#include <cmath>
#include <cstdlib>
#include <iomanip>
#include <optional>
#include <sstream>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace taviq {
namespace {
enum class Type { Number, String, Boolean };

struct FunctionSignature {
    std::string symbol;
    std::vector<Type> parameters;
    Type result{Type::Number};
};

struct Binding {
    std::string symbol;
    Type type{Type::Number};
    bool mutableLocal{false};
};

struct Context {
    std::vector<std::unordered_map<std::string, Binding>> scopes{{}};
    std::unordered_map<std::string, FunctionSignature> functions;
    std::optional<Type> returnType;
    std::ostringstream code;
    std::size_t nextSymbol{0};
    std::vector<std::string> errors;
};

bool typeFromName(const std::string& name, Type& type) {
    if (name == "number") { type = Type::Number; return true; }
    if (name == "string") { type = Type::String; return true; }
    if (name == "bool") { type = Type::Boolean; return true; }
    return false;
}

const char* cType(Type type) {
    switch (type) {
        case Type::Number: return "double";
        case Type::String: return "const char*";
        case Type::Boolean: return "int";
    }
    return "double";
}

const Binding* resolve(const Context& context, const std::string& name) {
    for (auto scope = context.scopes.rbegin(); scope != context.scopes.rend(); ++scope) {
        const auto found = scope->find(name);
        if (found != scope->end()) return &found->second;
    }
    return nullptr;
}

std::string quoteC(const std::string& value) {
    std::ostringstream output;
    output << '"';
    for (unsigned char character : value) {
        switch (character) {
            case '\\': output << "\\\\"; break;
            case '"': output << "\\\""; break;
            case '\n': output << "\\n"; break;
            case '\r': output << "\\r"; break;
            case '\t': output << "\\t"; break;
            case '\0': output << "\\000"; break;
            default:
                if (character < 32 || character >= 127) {
                    output << "\\" << std::oct << std::setw(3)
                           << std::setfill('0') << static_cast<int>(character) << std::dec;
                } else {
                    output << static_cast<char>(character);
                }
        }
    }
    output << '"';
    return output.str();
}

bool infer(const std::shared_ptr<Expr>& expression, const Context& context, Type& result) {
    if (!expression) {
        const_cast<Context&>(context).errors.push_back("native C backend: missing expression");
        return false;
    }
    const auto fail = [&](const std::string& message) {
        const_cast<Context&>(context).errors.push_back(message);
        return false;
    };

    switch (expression->kind) {
        case Expr::Kind::Number: result = Type::Number; return true;
        case Expr::Kind::String: result = Type::String; return true;
        case Expr::Kind::Boolean: result = Type::Boolean; return true;
        case Expr::Kind::Variable: {
            const auto* binding = resolve(context, expression->text);
            if (!binding) return fail("native C backend: undefined variable " + expression->text);
            result = binding->type;
            return true;
        }
        case Expr::Kind::Call: {
            if (expression->text == "len" || expression->text == "sqrt" ||
                expression->text == "abs") {
                if (expression->arguments.size() != 1)
                    return fail("native C backend: built-in function requires exactly one argument");
                Type argument;
                if (!infer(expression->arguments.front(), context, argument)) return false;
                if (expression->text == "len" && argument == Type::String) {
                    result = Type::Number;
                    return true;
                }
                if ((expression->text == "sqrt" || expression->text == "abs") &&
                    argument == Type::Number) {
                    result = Type::Number;
                    return true;
                }
                return fail("native C backend: unsupported built-in argument type " + expression->text);
            }
            const auto found = context.functions.find(expression->text);
            if (found == context.functions.end())
                return fail("native C backend: unknown function " + expression->text);
            const auto& signature = found->second;
            if (expression->arguments.size() != signature.parameters.size())
                return fail("native C backend: wrong argument count for function " + expression->text);
            for (std::size_t index = 0; index < expression->arguments.size(); ++index) {
                Type argument;
                if (!infer(expression->arguments[index], context, argument)) return false;
                if (argument != signature.parameters[index])
                    return fail("native C backend: argument type mismatch for function " + expression->text);
            }
            result = signature.result;
            return true;
        }
        case Expr::Kind::Unary: {
            Type argument;
            if (!infer(expression->right, context, argument)) return false;
            if (expression->op == '-' && argument == Type::Number) {
                result = Type::Number;
                return true;
            }
            if (expression->op == '!' && argument == Type::Boolean) {
                result = Type::Boolean;
                return true;
            }
            return fail("native C backend: invalid unary operand type");
        }
        case Expr::Kind::Binary: break;
    }

    Type left, right;
    if (!infer(expression->left, context, left) ||
        !infer(expression->right, context, right)) return false;
    switch (expression->op) {
        case '+':
            if (left == Type::Number && right == Type::Number) {
                result = Type::Number;
                return true;
            }
            if (left == Type::String && right == Type::String)
                return fail("native C backend: string concatenation is not yet lowered to C");
            return fail("native C backend: '+' operands must both be numbers or strings");
        case '-': case '*': case '/':
            if (left == Type::Number && right == Type::Number) {
                result = Type::Number;
                return true;
            }
            return fail("native C backend: arithmetic requires numbers");
        case '<': case 'l': case '>': case 'g':
            if (left == Type::Number && right == Type::Number) {
                result = Type::Boolean;
                return true;
            }
            return fail("native C backend: ordering comparisons require numbers");
        case 'q': case '!':
            if (left == right) {
                result = Type::Boolean;
                return true;
            }
            return fail("native C backend: equality operands must have the same type");
        case '&': case '|':
            if (left == Type::Boolean && right == Type::Boolean) {
                result = Type::Boolean;
                return true;
            }
            return fail("native C backend: logical operators require booleans");
        default: return fail("native C backend: unsupported binary operator");
    }
}

bool emitExpr(const std::shared_ptr<Expr>& expression, const Context& context,
              std::string& output) {
    if (!expression) return false;
    Type result;
    if (!infer(expression, context, result)) return false;

    switch (expression->kind) {
        case Expr::Kind::Number: {
            char* end = nullptr;
            const double value = std::strtod(expression->text.c_str(), &end);
            if (!end || *end != '\0' || !std::isfinite(value)) {
                const_cast<Context&>(context).errors.push_back(
                    "native C backend: invalid or non-finite number " + expression->text);
                return false;
            }
            output = expression->text;
            return true;
        }
        case Expr::Kind::String:
            output = quoteC(expression->text);
            return true;
        case Expr::Kind::Boolean:
            output = expression->boolean ? "1" : "0";
            return true;
        case Expr::Kind::Variable: {
            const auto* binding = resolve(context, expression->text);
            if (!binding) return false;
            output = binding->symbol;
            return true;
        }
        case Expr::Kind::Call: {
            if (expression->text == "len" || expression->text == "sqrt" ||
                expression->text == "abs") {
                std::string argument;
                if (!emitExpr(expression->arguments.front(), context, argument)) return false;
                if (expression->text == "len") output = "((double)strlen(" + argument + "))";
                else if (expression->text == "sqrt") output = "taviq_sqrt(" + argument + ")";
                else output = "fabs(" + argument + ")";
                return true;
            }
            const auto found = context.functions.find(expression->text);
            if (found == context.functions.end()) return false;
            output = found->second.symbol + "(";
            for (std::size_t index = 0; index < expression->arguments.size(); ++index) {
                std::string argument;
                if (!emitExpr(expression->arguments[index], context, argument)) return false;
                if (index) output += ", ";
                output += argument;
            }
            output += ")";
            return true;
        }
        case Expr::Kind::Unary: {
            std::string argument;
            if (!emitExpr(expression->right, context, argument)) return false;
            output = std::string("(") + (expression->op == '-' ? "-" : "!") +
                     "(" + argument + "))";
            return true;
        }
        case Expr::Kind::Binary: break;
    }

    std::string left, right;
    if (!emitExpr(expression->left, context, left) ||
        !emitExpr(expression->right, context, right)) return false;
    Type leftType;
    if (!infer(expression->left, context, leftType)) return false;
    switch (expression->op) {
        case '+': output = "((" + left + ") + (" + right + "))"; break;
        case '-': output = "((" + left + ") - (" + right + "))"; break;
        case '*': output = "((" + left + ") * (" + right + "))"; break;
        case '/': output = "taviq_div(" + left + "," + right + ")"; break;
        case '<': output = "((" + left + ") < (" + right + "))"; break;
        case 'l': output = "((" + left + ") <= (" + right + "))"; break;
        case '>': output = "((" + left + ") > (" + right + "))"; break;
        case 'g': output = "((" + left + ") >= (" + right + "))"; break;
        case '&': output = "((" + left + ") && (" + right + "))"; break;
        case '|': output = "((" + left + ") || (" + right + "))"; break;
        case 'q':
            output = leftType == Type::String
                ? "(strcmp(" + left + "," + right + ")==0)"
                : "((" + left + ") == (" + right + "))";
            break;
        case '!':
            output = leftType == Type::String
                ? "(strcmp(" + left + "," + right + ")!=0)"
                : "((" + left + ") != (" + right + "))";
            break;
        default: return false;
    }
    return true;
}

std::string indentation(std::size_t depth) {
    return std::string(depth * 4, ' ');
}

bool emitStatements(const std::vector<Statement>& statements, Context& context,
                    std::size_t depth, bool insideLoop);

bool emitBlock(const std::vector<Statement>& statements, Context& context,
               std::size_t depth, bool insideLoop) {
    context.scopes.emplace_back();
    context.code << indentation(depth) << "{\n";
    const bool result = emitStatements(statements, context, depth + 1, insideLoop);
    context.code << indentation(depth) << "}\n";
    context.scopes.pop_back();
    return result;
}

bool emitStatements(const std::vector<Statement>& statements, Context& context,
                    std::size_t depth, bool insideLoop) {
    bool ok = true;
    for (const auto& statement : statements) {
        const auto pad = indentation(depth);
        if (statement.kind == Statement::Kind::Let ||
            statement.kind == Statement::Kind::Var) {
            if (context.scopes.back().contains(statement.name)) {
                context.errors.push_back("native C backend: duplicate variable " + statement.name);
                ok = false;
                continue;
            }
            if (insideLoop && statement.kind == Statement::Kind::Let) {
                context.errors.push_back("native C backend: use var for declarations inside while loops");
                ok = false;
                continue;
            }
            Type type;
            if (!infer(statement.expression, context, type)) { ok = false; continue; }
            if (!statement.declaredType.empty()) {
                Type declared;
                if (!typeFromName(statement.declaredType, declared) || declared != type) {
                    context.errors.push_back("native C backend: annotation does not match initializer for " +
                                             statement.name);
                    ok = false;
                    continue;
                }
            }
            std::string expression;
            if (!emitExpr(statement.expression, context, expression)) { ok = false; continue; }
            const auto symbol = "taviq_v" + std::to_string(context.nextSymbol++);
            const bool immutable = statement.kind == Statement::Kind::Let;
            context.code << pad << "    ";
            if (immutable && type != Type::String) context.code << "const ";
            context.code << cType(type);
            if (immutable && type == Type::String) context.code << " const";
            context.code << " " << symbol << " = " << expression << ";\n";
            context.scopes.back().emplace(statement.name, Binding{symbol, type, !immutable});
        } else if (statement.kind == Statement::Kind::Assign) {
            const auto* binding = resolve(context, statement.name);
            if (!binding) {
                context.errors.push_back("native C backend: assignment to undefined variable " + statement.name);
                ok = false;
                continue;
            }
            if (!binding->mutableLocal) {
                context.errors.push_back("native C backend: cannot assign to immutable let " + statement.name);
                ok = false;
                continue;
            }
            Type type;
            if (!infer(statement.expression, context, type) || type != binding->type) {
                context.errors.push_back("native C backend: assignment type mismatch for " + statement.name);
                ok = false;
                continue;
            }
            std::string expression;
            if (!emitExpr(statement.expression, context, expression)) { ok = false; continue; }
            context.code << pad << "    " << binding->symbol << " = " << expression << ";\n";
        } else if (statement.kind == Statement::Kind::Display) {
            Type type;
            if (!infer(statement.expression, context, type)) { ok = false; continue; }
            std::string expression;
            if (!emitExpr(statement.expression, context, expression)) { ok = false; continue; }
            if (type == Type::Number)
                context.code << pad << "    printf(\"%.15g\\n\", (double)(" << expression << "));\n";
            else if (type == Type::Boolean)
                context.code << pad << "    puts((" << expression << ") ? \"true\" : \"false\");\n";
            else
                context.code << pad << "    puts(" << expression << ");\n";
        } else if (statement.kind == Statement::Kind::If) {
            Type type;
            if (!infer(statement.expression, context, type) || type != Type::Boolean) {
                context.errors.push_back("native C backend: if condition must be Boolean");
                ok = false;
                continue;
            }
            std::string condition;
            if (!emitExpr(statement.expression, context, condition)) { ok = false; continue; }
            context.code << pad << "    if (" << condition << ")\n";
            if (!emitBlock(statement.body, context, depth + 1, insideLoop)) ok = false;
            if (!statement.elseBody.empty()) {
                context.code << pad << "    else\n";
                if (!emitBlock(statement.elseBody, context, depth + 1, insideLoop)) ok = false;
            }
        } else if (statement.kind == Statement::Kind::While) {
            Type type;
            if (!infer(statement.expression, context, type) || type != Type::Boolean) {
                context.errors.push_back("native C backend: while condition must be Boolean");
                ok = false;
                continue;
            }
            std::string condition;
            if (!emitExpr(statement.expression, context, condition)) { ok = false; continue; }
            context.code << pad << "    while (" << condition << ")\n";
            if (!emitBlock(statement.body, context, depth + 1, true)) ok = false;
        } else if (statement.kind == Statement::Kind::Return) {
            if (!context.returnType) {
                context.errors.push_back("native C backend: return is only valid inside a function");
                ok = false;
                continue;
            }
            Type type;
            if (!infer(statement.expression, context, type) || type != *context.returnType) {
                context.errors.push_back("native C backend: function return type mismatch");
                ok = false;
                continue;
            }
            std::string expression;
            if (!emitExpr(statement.expression, context, expression)) { ok = false; continue; }
            context.code << pad << "    return " << expression << ";\n";
        }
    }
    return ok;
}

bool definitelyReturns(const std::vector<Statement>& statements) {
    for (const auto& statement : statements) {
        if (statement.kind == Statement::Kind::Return) return true;
        if (statement.kind == Statement::Kind::If && !statement.elseBody.empty() &&
            definitelyReturns(statement.body) && definitelyReturns(statement.elseBody)) return true;
    }
    return false;
}
} // namespace

bool emitCWithFunctions(const Program& program, std::string& output,
                        std::vector<std::string>& errors) {
    std::unordered_map<std::string, FunctionSignature> signatures;
    signatures.reserve(program.functions.size());

    for (std::size_t index = 0; index < program.functions.size(); ++index) {
        const auto& function = program.functions[index];
        FunctionSignature signature;
        signature.symbol = "taviq_fn" + std::to_string(index);
        if (!typeFromName(function.returnType, signature.result)) {
            errors.push_back("native C backend: unsupported return type for function " + function.name);
            continue;
        }
        bool valid = true;
        for (const auto& parameter : function.parameters) {
            Type type;
            if (!typeFromName(parameter.declaredType, type)) {
                errors.push_back("native C backend: unsupported parameter type in function " + function.name);
                valid = false;
                break;
            }
            signature.parameters.push_back(type);
        }
        if (!valid) continue;
        if (!signatures.emplace(function.name, std::move(signature)).second)
            errors.push_back("native C backend: duplicate function " + function.name);
    }
    if (!errors.empty()) return false;

    std::vector<std::string> definitions;
    definitions.reserve(program.functions.size());
    for (const auto& function : program.functions) {
        const auto found = signatures.find(function.name);
        if (found == signatures.end()) return false;
        const auto& signature = found->second;
        if (function.parameters.size() != signature.parameters.size()) {
            errors.push_back("native C backend: invalid parameter list for " + function.name);
            continue;
        }

        Context context;
        context.functions = signatures;
        context.returnType = signature.result;
        bool parametersValid = true;
        for (std::size_t index = 0; index < function.parameters.size(); ++index) {
            const auto& parameter = function.parameters[index];
            const std::string symbol = "taviq_p" + std::to_string(index);
            if (!context.scopes.front().emplace(
                    parameter.name, Binding{symbol, signature.parameters[index], false}).second) {
                errors.push_back("native C backend: duplicate parameter " + parameter.name);
                parametersValid = false;
            }
        }
        if (!parametersValid) continue;
        if (!emitStatements(function.body, context, 0, false)) {
            errors.insert(errors.end(), context.errors.begin(), context.errors.end());
            continue;
        }
        if (!context.errors.empty()) {
            errors.insert(errors.end(), context.errors.begin(), context.errors.end());
            continue;
        }
        if (!definitelyReturns(function.body)) {
            errors.push_back("native C backend: function does not return on every path: " + function.name);
            continue;
        }

        std::ostringstream definition;
        definition << "static " << cType(signature.result) << " " << signature.symbol << "(";
        for (std::size_t index = 0; index < signature.parameters.size(); ++index) {
            if (index) definition << ", ";
            definition << cType(signature.parameters[index]) << " taviq_p" << index;
        }
        if (signature.parameters.empty()) definition << "void";
        definition << ") {\n" << context.code.str() << "}\n";
        definitions.push_back(definition.str());
    }
    if (!errors.empty()) return false;

    Context mainContext;
    mainContext.functions = signatures;
    if (!emitStatements(program.statements, mainContext, 0, false) ||
        !mainContext.errors.empty()) {
        errors.insert(errors.end(), mainContext.errors.begin(), mainContext.errors.end());
        return false;
    }

    std::ostringstream result;
    result << "#include <math.h>\n#include <stdio.h>\n#include <stdlib.h>\n#include <string.h>\n"
           << "static double taviq_div(double a, double b) { if (b == 0.0) { "
           << "fputs(\"TAVIQ: division by zero\\n\", stderr); exit(1); } "
           << "double value = a / b; if (!isfinite(value)) { "
           << "fputs(\"TAVIQ: non-finite result\\n\", stderr); exit(1); } return value; }\n"
           << "static double taviq_sqrt(double a) { if (a < 0.0) { "
           << "fputs(\"TAVIQ: negative square root\\n\", stderr); exit(1); } "
           << "double value = sqrt(a); if (!isfinite(value)) { "
           << "fputs(\"TAVIQ: non-finite result\\n\", stderr); exit(1); } return value; }\n";
    for (const auto& function : program.functions) {
        const auto& signature = signatures.at(function.name);
        result << "static " << cType(signature.result) << " " << signature.symbol << "(";
        for (std::size_t index = 0; index < signature.parameters.size(); ++index) {
            if (index) result << ", ";
            result << cType(signature.parameters[index]) << " taviq_p" << index;
        }
        if (signature.parameters.empty()) result << "void";
        result << ");\n";
    }
    for (const auto& definition : definitions) result << definition;
    result << "int main(void) {\n" << mainContext.code.str() << "    return 0;\n}\n";
    output = result.str();
    return true;
}
} // namespace taviq
