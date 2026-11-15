#include "taviq/vm.hpp"
#include <cmath>
#include <ostream>
#include <utility>
#include <variant>
#include <vector>
namespace taviq {
int VM::run(const Bytecode& bytecode,std::ostream& output,std::string& error,std::size_t instructionBudget) const {
    error.clear();
    if (!verifyBytecode(bytecode,error)) return 1;
    if (bytecode.code.size()>instructionBudget) {
        error="QVM program exceeds instruction budget";
        return 1;
    }
    using Value=std::variant<double,std::string>;
    std::vector<Value> stack;
    std::vector<Value> locals(bytecode.locals.size());
    std::vector<bool> initialized(bytecode.locals.size(),false);
    const auto fail=[&](const std::string& message) { error=message; return 1; };
    const auto pop=[&](Value& value) {
        if (stack.empty()) return false;
        value=std::move(stack.back());
        stack.pop_back();
        return true;
    };
    for (const auto& ins : bytecode.code) {
        switch (ins.op) {
            case OpCode::PushNumber:
                if (!std::isfinite(ins.number)) return fail("QVM invalid non-finite number");
                stack.emplace_back(ins.number);
                break;
            case OpCode::PushString:
                stack.emplace_back(ins.text);
                break;
            case OpCode::LoadLocal:
                if (ins.slot>=locals.size() || !initialized[ins.slot])
                    return fail("QVM invalid or uninitialized local slot");
                stack.push_back(locals[ins.slot]);
                break;
            case OpCode::StoreLocal: {
                Value value;
                if (ins.slot>=locals.size() || initialized[ins.slot] || !pop(value))
                    return fail("QVM invalid, duplicate or uninitialized local store");
                locals[ins.slot]=std::move(value);
                initialized[ins.slot]=true;
                break;
            }
            case OpCode::Negate:
            case OpCode::SquareRoot:
            case OpCode::Absolute: {
                Value value;
                if (!pop(value) || !std::holds_alternative<double>(value))
                    return fail("QVM numeric unary operator requires a number");
                const double number=std::get<double>(value);
                double result=number;
                if (ins.op==OpCode::Negate) result=-number;
                else if (ins.op==OpCode::SquareRoot) {
                    if (number<0.0) return fail("QVM SQRT input must be non-negative");
                    result=std::sqrt(number);
                } else result=std::fabs(number);
                if (!std::isfinite(result)) return fail("QVM numeric unary result is not finite");
                stack.emplace_back(result);
                break;
            }
            case OpCode::Length: {
                Value value;
                if (!pop(value) || !std::holds_alternative<std::string>(value))
                    return fail("QVM LENGTH expects a string");
                stack.emplace_back(static_cast<double>(std::get<std::string>(value).size()));
                break;
            }
            case OpCode::Add:
            case OpCode::Subtract:
            case OpCode::Multiply:
            case OpCode::Divide: {
                if (stack.size()<2) return fail("QVM arithmetic stack underflow");
                Value rhs=std::move(stack.back()); stack.pop_back();
                Value lhs=std::move(stack.back()); stack.pop_back();
                if (ins.op==OpCode::Add && std::holds_alternative<std::string>(lhs) &&
                    std::holds_alternative<std::string>(rhs)) {
                    const auto& left=std::get<std::string>(lhs);
                    const auto& right=std::get<std::string>(rhs);
                    constexpr std::size_t MaxStringBytes=16*1024*1024;
                    if (left.size()>MaxStringBytes || right.size()>MaxStringBytes-left.size())
                        return fail("QVM string concatenation exceeds the v1 memory limit");
                    std::string joined;
                    joined.reserve(left.size()+right.size());
                    joined.append(left);
                    joined.append(right);
                    stack.emplace_back(std::move(joined));
                    break;
                }
                if (!std::holds_alternative<double>(lhs) || !std::holds_alternative<double>(rhs))
                    return fail("QVM arithmetic operands must be numbers");
                const double a=std::get<double>(lhs), b=std::get<double>(rhs);
                double result=0.0;
                if (ins.op==OpCode::Add) result=a+b;
                else if (ins.op==OpCode::Subtract) result=a-b;
                else if (ins.op==OpCode::Multiply) result=a*b;
                else {
                    if (b==0.0) return fail("division by zero");
                    result=a/b;
                }
                if (!std::isfinite(result)) return fail("numeric result is not finite");
                stack.emplace_back(result);
                break;
            }
            case OpCode::Print: {
                Value value;
                if (!pop(value)) return fail("QVM PRINT stack underflow");
                if (std::holds_alternative<double>(value)) output<<std::get<double>(value);
                else output<<std::get<std::string>(value);
                output<<'\n';
                if (!output) return fail("failed to write program output");
                break;
            }
            case OpCode::Halt:
                return 0;
            default:
                return fail("unknown opcode in QVM");
        }
        if (stack.size()>65536) return fail("QVM operand stack limit exceeded");
    }
    return fail("QVM bytecode has no HALT instruction");
}
}
