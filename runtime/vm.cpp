#include "taviq/vm.hpp"
#include <cmath>
#include <ostream>
#include <utility>
#include <variant>
#include <vector>
namespace taviq {
int VM::run(const Bytecode& bytecode,std::ostream& output,std::string& error,
            std::size_t instructionBudget,const TraceCallback& trace)const {
    error.clear();
    if(!verifyBytecode(bytecode,error))return 1;
    using Value=std::variant<double,std::string,bool>;
    struct Frame {
        bool main{false};
        std::size_t functionIndex{};
        std::size_t start{},pc{},end{},returnAddress{},stackBase{};
        std::vector<Value> locals;
        std::vector<bool> initialized,mutableLocals;
    };
    constexpr std::size_t MaxStringBytes=16*1024*1024;
    constexpr std::size_t MaxStackValues=65'536;
    constexpr std::size_t MaxCallDepth=256;
    constexpr std::size_t MaxTotalLocals=1'000'000;
    const auto fail=[&](const std::string& message){error=message;return 1;};
    const auto isString=[](const Value& value){return std::holds_alternative<std::string>(value);};
    std::vector<Value> stack;
    std::vector<Frame> frames;
    Frame mainFrame;
    mainFrame.main=true;
    mainFrame.start=0;
    mainFrame.end=bytecode.formatVersion==3?bytecode.mainEnd:bytecode.code.size();
    mainFrame.locals.resize(bytecode.locals.size());
    mainFrame.initialized.resize(bytecode.locals.size(),false);
    mainFrame.mutableLocals.resize(bytecode.locals.size(),false);
    frames.push_back(std::move(mainFrame));
    std::size_t steps=0,totalLocals=bytecode.locals.size();

    const auto memoryUsed=[&]() -> std::size_t {
        std::size_t total=0;
        for(const auto& value:stack)if(isString(value)) {
            const auto size=std::get<std::string>(value).size();
            if(size>MaxStringBytes-total)return MaxStringBytes+1;
            total+=size;
        }
        for(const auto& frame:frames) {
            for(std::size_t i=0;i<frame.locals.size();++i) {
                if(!frame.initialized[i]||!isString(frame.locals[i]))continue;
                const auto size=std::get<std::string>(frame.locals[i]).size();
                if(size>MaxStringBytes-total)return MaxStringBytes+1;
                total+=size;
            }
        }
        return total;
    };
    const auto pop=[&](std::size_t stackBase,Value& value) {
        if(stack.size()<=stackBase)return false;
        value=std::move(stack.back());stack.pop_back();return true;
    };

    std::size_t pc=0;
    while(!frames.empty()) {
        if(steps++>=instructionBudget)return fail("QVM execution instruction budget exceeded");
        const auto frameIndex=frames.size()-1;
        const auto frameBase=frames[frameIndex].stackBase;
        const auto& ins=bytecode.code[pc];
        if(trace) {
            try{trace(pc,ins);}
            catch(const std::exception& ex){return fail(std::string("QVM trace callback failed: ")+ex.what());}
            catch(...){return fail("QVM trace callback failed");}
        }
        bool advancePc=true;
        switch(ins.op) {
            case OpCode::PushNumber:
                if(!std::isfinite(ins.number))return fail("QVM invalid non-finite number");
                stack.emplace_back(ins.number);
                break;
            case OpCode::PushString:
                stack.emplace_back(ins.text);
                if(memoryUsed()>MaxStringBytes)return fail("QVM live string memory exceeds 16 MiB");
                break;
            case OpCode::PushBoolean:
                stack.emplace_back(ins.slot!=0);
                break;
            case OpCode::LoadLocal: {
                auto& frame=frames[frameIndex];
                if(ins.slot>=frame.locals.size()||!frame.initialized[ins.slot])
                    return fail("QVM invalid or uninitialized local slot");
                stack.push_back(frame.locals[ins.slot]);
                if(memoryUsed()>MaxStringBytes)return fail("QVM live string memory exceeds 16 MiB");
                break;
            }
            case OpCode::StoreLocal:
            case OpCode::StoreLocalMutable: {
                auto& frame=frames[frameIndex];
                Value value;
                if(ins.slot>=frame.locals.size()||!pop(frameBase,value))
                    return fail("QVM invalid local store or operand-stack underflow");
                if(ins.op==OpCode::StoreLocal) {
                    if(frame.initialized[ins.slot])return fail("QVM immutable local is initialized more than once");
                    frame.mutableLocals[ins.slot]=false;
                } else {
                    if(frame.initialized[ins.slot]&&!frame.mutableLocals[ins.slot])
                        return fail("QVM mutable store overwrites immutable local");
                    if(frame.initialized[ins.slot]&&frame.locals[ins.slot].index()!=value.index())
                        return fail("QVM mutable local assignment changes type");
                    frame.mutableLocals[ins.slot]=true;
                }
                frame.locals[ins.slot]=std::move(value);
                frame.initialized[ins.slot]=true;
                if(memoryUsed()>MaxStringBytes)return fail("QVM live string memory exceeds 16 MiB");
                break;
            }
            case OpCode::Negate:
            case OpCode::SquareRoot:
            case OpCode::Absolute: {
                Value value;
                if(!pop(frameBase,value)||!std::holds_alternative<double>(value))
                    return fail("QVM numeric unary operation requires a number");
                const double number=std::get<double>(value);
                double result=number;
                if(ins.op==OpCode::Negate)result=-number;
                else if(ins.op==OpCode::SquareRoot) {
                    if(number<0.0)return fail("QVM SQRT input must be non-negative");
                    result=std::sqrt(number);
                } else result=std::fabs(number);
                if(!std::isfinite(result))return fail("QVM numeric unary result is not finite");
                stack.emplace_back(result);
                break;
            }
            case OpCode::LogicalNot: {
                Value value;
                if(!pop(frameBase,value)||!std::holds_alternative<bool>(value))
                    return fail("QVM logical NOT expects a Boolean");
                stack.emplace_back(!std::get<bool>(value));
                break;
            }
            case OpCode::Length: {
                Value value;
                if(!pop(frameBase,value)||!std::holds_alternative<std::string>(value))
                    return fail("QVM LENGTH expects a string");
                stack.emplace_back(static_cast<double>(std::get<std::string>(value).size()));
                break;
            }
            case OpCode::Add:
            case OpCode::Subtract:
            case OpCode::Multiply:
            case OpCode::Divide: {
                if(stack.size()<frameBase+2)return fail("QVM arithmetic stack underflow");
                Value rhs=std::move(stack.back());stack.pop_back();
                Value lhs=std::move(stack.back());stack.pop_back();
                if(ins.op==OpCode::Add&&std::holds_alternative<std::string>(lhs)&&
                   std::holds_alternative<std::string>(rhs)) {
                    const auto& left=std::get<std::string>(lhs);
                    const auto& right=std::get<std::string>(rhs);
                    if(left.size()>MaxStringBytes||right.size()>MaxStringBytes-left.size())
                        return fail("QVM string concatenation exceeds 16 MiB");
                    const auto before=memoryUsed();
                    if(left.size()+right.size()>MaxStringBytes-before)
                        return fail("QVM string concatenation peak memory exceeds 16 MiB");
                    std::string joined;joined.reserve(left.size()+right.size());
                    joined.append(left);joined.append(right);stack.emplace_back(std::move(joined));
                    if(memoryUsed()>MaxStringBytes)return fail("QVM live string memory exceeds 16 MiB");
                    break;
                }
                if(!std::holds_alternative<double>(lhs)||!std::holds_alternative<double>(rhs))
                    return fail("QVM arithmetic operands must be numbers");
                const double left=std::get<double>(lhs),right=std::get<double>(rhs);
                double result=0.0;
                if(ins.op==OpCode::Add)result=left+right;
                else if(ins.op==OpCode::Subtract)result=left-right;
                else if(ins.op==OpCode::Multiply)result=left*right;
                else {
                    if(right==0.0)return fail("division by zero");
                    result=left/right;
                }
                if(!std::isfinite(result))return fail("numeric result is not finite");
                stack.emplace_back(result);
                break;
            }
            case OpCode::Equal:
            case OpCode::NotEqual: {
                if(stack.size()<frameBase+2)return fail("QVM equality stack underflow");
                Value rhs=std::move(stack.back());stack.pop_back();
                Value lhs=std::move(stack.back());stack.pop_back();
                const bool equal=lhs==rhs;
                stack.emplace_back(ins.op==OpCode::Equal?equal:!equal);
                break;
            }
            case OpCode::Less:
            case OpCode::LessEqual:
            case OpCode::Greater:
            case OpCode::GreaterEqual: {
                if(stack.size()<frameBase+2)return fail("QVM comparison stack underflow");
                Value rhs=std::move(stack.back());stack.pop_back();
                Value lhs=std::move(stack.back());stack.pop_back();
                if(!std::holds_alternative<double>(lhs)||!std::holds_alternative<double>(rhs))
                    return fail("QVM ordering comparisons require numbers");
                const double left=std::get<double>(lhs),right=std::get<double>(rhs);
                bool result=false;
                if(ins.op==OpCode::Less)result=left<right;
                else if(ins.op==OpCode::LessEqual)result=left<=right;
                else if(ins.op==OpCode::Greater)result=left>right;
                else result=left>=right;
                stack.emplace_back(result);
                break;
            }
            case OpCode::Print: {
                Value value;
                if(!pop(frameBase,value))return fail("QVM PRINT stack underflow");
                if(std::holds_alternative<double>(value))output<<std::get<double>(value);
                else if(std::holds_alternative<std::string>(value))output<<std::get<std::string>(value);
                else output<<(std::get<bool>(value)?"true":"false");
                output<<'\n';
                if(!output)return fail("failed to write program output");
                break;
            }
            case OpCode::Jump:
                if(ins.slot<frames[frameIndex].start||ins.slot>=frames[frameIndex].end)
                    return fail("QVM jump target is outside the current frame");
                pc=ins.slot;advancePc=false;
                break;
            case OpCode::JumpIfFalse: {
                Value value;
                if(!pop(frameBase,value)||!std::holds_alternative<bool>(value))
                    return fail("QVM conditional jump expects a Boolean");
                if(!std::get<bool>(value)) {
                    if(ins.slot<frames[frameIndex].pc||ins.slot>=frames[frameIndex].end)
                        return fail("QVM conditional jump target is outside the current frame");
                    pc=ins.slot;advancePc=false;
                }
                break;
            }
            case OpCode::Call: {
                if(bytecode.formatVersion!=3||ins.slot>=bytecode.functions.size())
                    return fail("QVM CALL target is invalid");
                if(frames.size()>=MaxCallDepth)return fail("QVM maximum call depth exceeded");
                const auto& function=bytecode.functions[ins.slot];
                if(ins.argumentCount!=function.parameterTypes.size()||
                   stack.size()<frameBase+ins.argumentCount)
                    return fail("QVM CALL argument count or stack is invalid");
                if(function.locals.size()>MaxTotalLocals-totalLocals)
                    return fail("QVM active call frames exceed local-slot budget");
                std::vector<Value> arguments(ins.argumentCount);
                for(std::size_t i=ins.argumentCount;i>0;--i) {
                    arguments[i-1]=std::move(stack.back());
                    stack.pop_back();
                }
                Frame callee;
                callee.main=false;
                callee.functionIndex=ins.slot;
                callee.start=function.entry;
                callee.pc=function.entry;
                callee.end=function.end;
                callee.returnAddress=pc+1;
                callee.stackBase=stack.size();
                callee.locals.resize(function.locals.size());
                callee.initialized.resize(function.locals.size(),false);
                callee.mutableLocals.resize(function.locals.size(),false);
                for(std::size_t i=0;i<arguments.size();++i) {
                    callee.locals[i]=std::move(arguments[i]);
                    callee.initialized[i]=true;
                    callee.mutableLocals[i]=false;
                }
                totalLocals+=callee.locals.size();
                frames.push_back(std::move(callee));
                pc=function.entry;
                advancePc=false;
                if(memoryUsed()>MaxStringBytes)return fail("QVM active call frames exceed 16 MiB string memory");
                break;
            }
            case OpCode::Return: {
                if(frames[frameIndex].main)return fail("QVM RETURN cannot exit main");
                if(stack.size()!=frameBase+1)return fail("QVM RETURN requires exactly one result value");
                Value result=std::move(stack.back());stack.pop_back();
                const auto returnAddress=frames[frameIndex].returnAddress;
                totalLocals-=frames[frameIndex].locals.size();
                frames.pop_back();
                if(frames.empty())return fail("QVM call stack lost the main frame");
                stack.push_back(std::move(result));
                pc=returnAddress;
                advancePc=false;
                if(memoryUsed()>MaxStringBytes)return fail("QVM returned value exceeds memory budget");
                break;
            }
            case OpCode::Halt:
                if(!frames[frameIndex].main)return fail("QVM HALT cannot terminate a function frame");
                if(stack.size()!=frameBase)return fail("QVM HALT requires an empty main operand stack");
                return 0;
            default:
                return fail("unknown opcode in QVM");
        }
        if(stack.size()>MaxStackValues)return fail("QVM operand stack limit exceeded");
        if(advancePc)++pc;
        if(frames.empty())return fail("QVM call stack unexpectedly became empty");
        if(pc>=frames.back().end)return fail("QVM instruction pointer left the active frame");
    }
    return fail("QVM terminated without HALT");
}
}
