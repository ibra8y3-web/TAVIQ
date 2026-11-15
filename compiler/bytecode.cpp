#include "taviq/bytecode.hpp"
#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstdio>
#include <deque>
#include <istream>
#include <optional>
#include <ostream>
#include <unordered_set>
#include <utility>
namespace taviq {
namespace {
constexpr std::size_t MaxInstructions=1'000'000;
constexpr std::size_t MaxLocals=65'536;
constexpr std::size_t MaxFunctions=65'536;
constexpr std::size_t MaxString=1*1024*1024;
constexpr std::size_t MaxEncoded=16*1024*1024;
constexpr std::size_t MaxLive=16*1024*1024;

struct StackValue { ValueType type{}; std::size_t stringBytes{}; };
struct LocalState {
    bool mayInit{false},definite{false},mutableLocal{false};
    ValueType type{ValueType::Number};
    std::size_t stringBytes{};
};
struct FlowState { std::vector<StackValue> stack; std::vector<LocalState> locals; };

void putU32(std::ostream& out,std::uint32_t value) {
    for(int i=0;i<4;++i)out.put(static_cast<char>((value>>(i*8))&0xff));
}
bool getU32(std::istream& in,std::uint32_t& value) {
    value=0;
    for(int i=0;i<4;++i) {
        const int c=in.get();
        if(c==EOF)return false;
        value|=static_cast<std::uint32_t>(static_cast<unsigned char>(c))<<(i*8);
    }
    return true;
}
void putU64(std::ostream& out,std::uint64_t value) {
    for(int i=0;i<8;++i)out.put(static_cast<char>((value>>(i*8))&0xff));
}
bool getU64(std::istream& in,std::uint64_t& value) {
    value=0;
    for(int i=0;i<8;++i) {
        const int c=in.get();
        if(c==EOF)return false;
        value|=static_cast<std::uint64_t>(static_cast<unsigned char>(c))<<(i*8);
    }
    return true;
}
bool putString(std::ostream& out,const std::string& value) {
    if(value.size()>MaxString)return false;
    putU32(out,static_cast<std::uint32_t>(value.size()));
    out.write(value.data(),static_cast<std::streamsize>(value.size()));
    return static_cast<bool>(out);
}
bool getString(std::istream& in,std::string& value,std::size_t& total) {
    std::uint32_t size{};
    if(!getU32(in,size)||size>MaxString||size>MaxEncoded-total)return false;
    value.resize(size);
    if(!in.read(value.data(),static_cast<std::streamsize>(size)))return false;
    total+=size;
    return true;
}
bool validType(ValueType type) {
    return type==ValueType::Number||type==ValueType::String||type==ValueType::Boolean;
}
bool validOpcode(std::uint8_t raw,std::uint32_t version) {
    if(raw<1)return false;
    if(version==1)return raw<=14;
    if(version==2)return raw<=25;
    if(version==3)return raw<=27;
    return false;
}
const char* name(OpCode op) {
    switch(op) {
        case OpCode::PushNumber:return "PUSH_NUMBER";
        case OpCode::PushString:return "PUSH_STRING";
        case OpCode::LoadLocal:return "LOAD_LOCAL";
        case OpCode::StoreLocal:return "STORE_LOCAL";
        case OpCode::Add:return "ADD";
        case OpCode::Subtract:return "SUB";
        case OpCode::Multiply:return "MUL";
        case OpCode::Divide:return "DIV";
        case OpCode::Negate:return "NEG";
        case OpCode::Print:return "PRINT";
        case OpCode::Halt:return "HALT";
        case OpCode::Length:return "LENGTH";
        case OpCode::SquareRoot:return "SQRT";
        case OpCode::Absolute:return "ABS";
        case OpCode::PushBoolean:return "PUSH_BOOLEAN";
        case OpCode::Equal:return "EQUAL";
        case OpCode::NotEqual:return "NOT_EQUAL";
        case OpCode::Less:return "LESS";
        case OpCode::LessEqual:return "LESS_EQUAL";
        case OpCode::Greater:return "GREATER";
        case OpCode::GreaterEqual:return "GREATER_EQUAL";
        case OpCode::LogicalNot:return "LOGICAL_NOT";
        case OpCode::Jump:return "JUMP";
        case OpCode::JumpIfFalse:return "JUMP_IF_FALSE";
        case OpCode::StoreLocalMutable:return "STORE_LOCAL_MUTABLE";
        case OpCode::Call:return "CALL";
        case OpCode::Return:return "RETURN";
    }
    return "INVALID";
}
std::size_t liveBytes(const FlowState& state) {
    std::size_t total=0;
    for(const auto& local:state.locals)if(local.mayInit) {
        if(local.stringBytes>MaxLive-total)return MaxLive+1;
        total+=local.stringBytes;
    }
    for(const auto& value:state.stack) {
        if(value.stringBytes>MaxLive-total)return MaxLive+1;
        total+=value.stringBytes;
    }
    return total;
}
bool sameState(const FlowState& a,const FlowState& b) {
    if(a.stack.size()!=b.stack.size()||a.locals.size()!=b.locals.size())return false;
    for(std::size_t i=0;i<a.stack.size();++i)
        if(a.stack[i].type!=b.stack[i].type||a.stack[i].stringBytes!=b.stack[i].stringBytes)return false;
    for(std::size_t i=0;i<a.locals.size();++i) {
        const auto& x=a.locals[i];const auto& y=b.locals[i];
        if(x.mayInit!=y.mayInit||x.definite!=y.definite||x.mutableLocal!=y.mutableLocal||
           x.type!=y.type||x.stringBytes!=y.stringBytes)return false;
    }
    return true;
}

bool verifySegment(const Bytecode& bytecode,std::size_t begin,std::size_t end,
                   const std::vector<std::string>& localNames,
                   const std::vector<ValueType>& parameters,
                   const ValueType* returnType,bool isMain,std::string& error) {
    const auto fail=[&](const std::string& message){error=message;return false;};
    if(begin>=end||end>bytecode.code.size())return fail("QVM function code range is invalid");
    if(localNames.size()>MaxLocals||parameters.size()>localNames.size())
        return fail("QVM function local or parameter count exceeds the limit");
    FlowState initial;
    initial.locals.resize(localNames.size());
    for(std::size_t i=0;i<parameters.size();++i) {
        if(!validType(parameters[i]))return fail("QVM function has an invalid parameter type");
        initial.locals[i]={true,true,false,parameters[i],0};
    }
    std::vector<std::optional<FlowState>> incoming(end-begin);
    incoming[0]=initial;
    std::deque<std::size_t> work{begin};
    bool reachedTerminal=false;

    const auto enqueue=[&](std::size_t target,const FlowState& candidate,std::size_t from)->bool {
        if(target<begin||target>=end) {
            error="QVM branch target leaves its function";return false;
        }
        if(liveBytes(candidate)>MaxLive) {
            error="QVM live string memory exceeds 16 MiB";return false;
        }
        auto& slot=incoming[target-begin];
        if(!slot) {
            slot=candidate;work.push_back(target);return true;
        }
        FlowState merged=*slot;
        if(merged.stack.size()!=candidate.stack.size()) {
            error="QVM control-flow paths merge with different stack heights";return false;
        }
        for(std::size_t i=0;i<merged.stack.size();++i) {
            auto& left=merged.stack[i];const auto& right=candidate.stack[i];
            if(left.type!=right.type) {
                error="QVM control-flow paths merge incompatible stack types";return false;
            }
            if(left.stringBytes!=right.stringBytes) {
                if(from>=target) {
                    error="QVM loop carries a string with an unbounded or varying size";return false;
                }
                left.stringBytes=std::max(left.stringBytes,right.stringBytes);
            }
        }
        for(std::size_t i=0;i<merged.locals.size();++i) {
            auto& left=merged.locals[i];const auto& right=candidate.locals[i];
            if(left.mayInit&&right.mayInit) {
                if(left.type!=right.type) {
                    error="QVM control-flow paths assign incompatible local types";return false;
                }
                if(left.mutableLocal!=right.mutableLocal) {
                    error="QVM control-flow paths disagree about local mutability";return false;
                }
                if(left.stringBytes!=right.stringBytes) {
                    if(from>=target) {
                        error="QVM loop carries a string local with varying size";return false;
                    }
                    left.stringBytes=std::max(left.stringBytes,right.stringBytes);
                }
                left.definite=left.definite&&right.definite;
            } else if(left.mayInit||right.mayInit) {
                const auto initialized=left.mayInit?left:right;
                left.mayInit=true;left.definite=false;left.mutableLocal=initialized.mutableLocal;
                left.type=initialized.type;left.stringBytes=initialized.stringBytes;
            } else {
                left=LocalState{};
            }
        }
        if(liveBytes(merged)>MaxLive) {
            error="QVM live string memory exceeds 16 MiB at control-flow merge";return false;
        }
        if(!sameState(*slot,merged)) {
            slot=std::move(merged);work.push_back(target);
        }
        return true;
    };

    while(!work.empty()) {
        const auto pc=work.front();work.pop_front();
        FlowState state=*incoming[pc-begin];
        const auto& ins=bytecode.code[pc];
        const auto pop=[&](StackValue& value) {
            if(state.stack.empty())return false;
            value=state.stack.back();state.stack.pop_back();return true;
        };
        const auto push=[&](ValueType type,std::size_t bytes=0) {
            state.stack.push_back({type,bytes});
            return liveBytes(state)<=MaxLive;
        };
        bool fallsThrough=true;
        switch(ins.op) {
            case OpCode::PushNumber:
                if(!push(ValueType::Number))return fail("QVM live string memory exceeds 16 MiB");
                break;
            case OpCode::PushString:
                if(!push(ValueType::String,ins.text.size()))return fail("QVM live string memory exceeds 16 MiB");
                break;
            case OpCode::PushBoolean:
                if(!push(ValueType::Boolean))return fail("QVM live string memory exceeds 16 MiB");
                break;
            case OpCode::LoadLocal: {
                if(ins.slot>=state.locals.size()||!state.locals[ins.slot].definite)
                    return fail("QVM loads an invalid, uninitialized or path-dependent local");
                const auto& local=state.locals[ins.slot];
                if(!push(local.type,local.stringBytes))return fail("QVM live string memory exceeds 16 MiB");
                break;
            }
            case OpCode::StoreLocal:
            case OpCode::StoreLocalMutable: {
                if(ins.slot>=state.locals.size())return fail("QVM store references an invalid local");
                StackValue value{};
                if(!pop(value))return fail("QVM local store underflows the operand stack");
                auto& local=state.locals[ins.slot];
                if(ins.op==OpCode::StoreLocal) {
                    if(local.mayInit)return fail("QVM immutable local is initialized more than once");
                    local={true,true,false,value.type,value.stringBytes};
                } else {
                    if(local.mayInit&&!local.mutableLocal)
                        return fail("QVM mutable store attempts to overwrite an immutable local");
                    if(local.mayInit&&local.type!=value.type)
                        return fail("QVM mutable local assignment changes type");
                    local={true,true,true,value.type,value.stringBytes};
                }
                if(liveBytes(state)>MaxLive)return fail("QVM live string memory exceeds 16 MiB");
                break;
            }
            case OpCode::Add:
            case OpCode::Subtract:
            case OpCode::Multiply:
            case OpCode::Divide: {
                if(state.stack.size()<2)return fail("QVM arithmetic instruction underflows the stack");
                const auto rhs=state.stack.back(),lhs=state.stack[state.stack.size()-2];
                if(ins.op==OpCode::Add&&lhs.type==ValueType::String&&rhs.type==ValueType::String) {
                    const auto before=liveBytes(state);
                    if(lhs.stringBytes>MaxLive-rhs.stringBytes)
                        return fail("QVM string concatenation exceeds 16 MiB");
                    const auto resultBytes=lhs.stringBytes+rhs.stringBytes;
                    if(resultBytes>MaxLive-before)
                        return fail("QVM string concatenation peak memory exceeds 16 MiB");
                    StackValue discarded{};pop(discarded);pop(discarded);
                    if(!push(ValueType::String,resultBytes))return fail("QVM live string memory exceeds 16 MiB");
                    break;
                }
                if(lhs.type!=ValueType::Number||rhs.type!=ValueType::Number)
                    return fail("QVM arithmetic operands must be numbers (except string + string)");
                StackValue discarded{};pop(discarded);pop(discarded);
                if(!push(ValueType::Number))return fail("QVM live string memory exceeds 16 MiB");
                break;
            }
            case OpCode::Negate:
            case OpCode::SquareRoot:
            case OpCode::Absolute: {
                StackValue value{};
                if(!pop(value)||value.type!=ValueType::Number)
                    return fail("QVM numeric unary operation requires a number");
                if(!push(ValueType::Number))return fail("QVM live string memory exceeds 16 MiB");
                break;
            }
            case OpCode::Length: {
                StackValue value{};
                if(!pop(value)||value.type!=ValueType::String)return fail("QVM LENGTH requires a string");
                if(!push(ValueType::Number))return fail("QVM live string memory exceeds 16 MiB");
                break;
            }
            case OpCode::LogicalNot: {
                StackValue value{};
                if(!pop(value)||value.type!=ValueType::Boolean)return fail("QVM NOT requires a Boolean");
                if(!push(ValueType::Boolean))return fail("QVM live string memory exceeds 16 MiB");
                break;
            }
            case OpCode::Equal:
            case OpCode::NotEqual: {
                if(state.stack.size()<2)return fail("QVM equality instruction underflows the stack");
                const auto rhs=state.stack.back(),lhs=state.stack[state.stack.size()-2];
                if(lhs.type!=rhs.type)return fail("QVM equality operands must have matching types");
                StackValue discarded{};pop(discarded);pop(discarded);
                if(!push(ValueType::Boolean))return fail("QVM live string memory exceeds 16 MiB");
                break;
            }
            case OpCode::Less:
            case OpCode::LessEqual:
            case OpCode::Greater:
            case OpCode::GreaterEqual: {
                if(state.stack.size()<2)return fail("QVM comparison instruction underflows the stack");
                const auto rhs=state.stack.back(),lhs=state.stack[state.stack.size()-2];
                if(lhs.type!=ValueType::Number||rhs.type!=ValueType::Number)
                    return fail("QVM ordering comparisons require numbers");
                StackValue discarded{};pop(discarded);pop(discarded);
                if(!push(ValueType::Boolean))return fail("QVM live string memory exceeds 16 MiB");
                break;
            }
            case OpCode::Call: {
                if(bytecode.formatVersion!=3||ins.slot>=bytecode.functions.size())
                    return fail("QVM CALL references an invalid function");
                const auto& target=bytecode.functions[ins.slot];
                if(ins.argumentCount!=target.parameterTypes.size())
                    return fail("QVM CALL argument count does not match function signature");
                if(state.stack.size()<ins.argumentCount)
                    return fail("QVM CALL underflows the operand stack");
                for(std::size_t i=ins.argumentCount;i>0;--i) {
                    StackValue argument{};
                    if(!pop(argument)||argument.type!=target.parameterTypes[i-1])
                        return fail("QVM CALL argument type does not match function signature");
                }
                if(!push(target.returnType))return fail("QVM live string memory exceeds 16 MiB");
                break;
            }
            case OpCode::Print: {
                StackValue printed{};
                if(!pop(printed))return fail("QVM PRINT underflows the operand stack");
                break;
            }
            case OpCode::Jump:
                if(bytecode.formatVersion<2)return fail("QVM JUMP is not supported in this bytecode version");
                fallsThrough=false;
                if(!enqueue(ins.slot,state,pc))return false;
                break;
            case OpCode::JumpIfFalse: {
                if(bytecode.formatVersion<2)return fail("QVM conditional jump is not supported in this version");
                StackValue condition{};
                if(!pop(condition)||condition.type!=ValueType::Boolean)
                    return fail("QVM conditional jump requires a Boolean");
                if(!enqueue(ins.slot,state,pc))return false;
                break;
            }
            case OpCode::Halt:
                if(!isMain||pc+1!=end||!state.stack.empty())
                    return fail("QVM HALT is only valid at the end of main with an empty stack");
                reachedTerminal=true;fallsThrough=false;
                break;
            case OpCode::Return: {
                if(isMain||!returnType)return fail("QVM RETURN is invalid in main");
                StackValue value{};
                if(!pop(value)||value.type!=*returnType)
                    return fail("QVM RETURN value does not match function return type");
                if(!state.stack.empty())return fail("QVM RETURN requires an otherwise empty operand stack");
                reachedTerminal=true;fallsThrough=false;
                break;
            }
            default:
                return fail("QVM contains an invalid opcode for this bytecode version");
        }
        if(state.stack.size()>MaxLocals)return fail("QVM operand stack exceeds 65,536 values");
        if(liveBytes(state)>MaxLive)return fail("QVM live string memory exceeds 16 MiB");
        if(fallsThrough) {
            if(pc+1>=end)return fail("QVM function falls through its end without RETURN");
            if(!enqueue(pc+1,state,pc))return false;
        }
    }
    if(!reachedTerminal)return fail(isMain?"QVM main HALT is unreachable":"QVM function RETURN is unreachable");
    return true;
}

bool verifyLegacy(const Bytecode& bytecode,std::string& error) {
    if(bytecode.code.empty()||bytecode.code.size()>MaxInstructions) {
        error="QVM instruction count is empty or exceeds limit";return false;
    }
    if(bytecode.locals.size()>MaxLocals){error="QVM local table exceeds limit";return false;}
    std::size_t encoded=0;
    std::unordered_set<std::string> names;
    for(const auto& local:bytecode.locals) {
        if(local.empty()||local.size()>MaxString||!names.insert(local).second) {
            error="QVM local names must be non-empty, bounded and unique";return false;
        }
        if(local.size()>MaxEncoded-encoded){error="QVM local-name table exceeds encoded limit";return false;}
        encoded+=local.size();
    }
    for(std::size_t pc=0;pc<bytecode.code.size();++pc) {
        const auto& ins=bytecode.code[pc];
        if(ins.text.size()>MaxString||ins.text.size()>MaxEncoded-encoded) {
            error="QVM string operands exceed encoded limit";return false;
        }
        encoded+=ins.text.size();
        const auto raw=static_cast<std::uint8_t>(ins.op);
        if(!validOpcode(raw,bytecode.formatVersion)){error="QVM contains an unknown opcode";return false;}
        if(ins.op==OpCode::PushNumber&&!std::isfinite(ins.number)) {
            error="QVM contains a non-finite numeric constant";return false;
        }
        if(ins.op==OpCode::PushBoolean&&ins.slot>1){error="QVM Boolean constant must be zero or one";return false;}
        if((ins.op==OpCode::Jump||ins.op==OpCode::JumpIfFalse)&&
           (bytecode.formatVersion<2||ins.slot>=bytecode.code.size())) {
            error="QVM jump target is outside instruction stream";return false;
        }
        if(ins.op==OpCode::Halt&&pc+1!=bytecode.code.size()){error="QVM HALT must be final";return false;}
    }
    if(bytecode.code.back().op!=OpCode::Halt){error="QVM bytecode has no final HALT";return false;}
    if(bytecode.formatVersion==1) {
        for(const auto& ins:bytecode.code) {
            if(ins.op==OpCode::Jump||ins.op==OpCode::JumpIfFalse||ins.op==OpCode::PushBoolean||
               ins.op==OpCode::LogicalNot||ins.op==OpCode::Equal||ins.op==OpCode::NotEqual||
               ins.op==OpCode::Less||ins.op==OpCode::LessEqual||ins.op==OpCode::Greater||
               ins.op==OpCode::GreaterEqual||ins.op==OpCode::StoreLocalMutable||ins.op==OpCode::Call||
               ins.op==OpCode::Return) {
                error="QVM v1 contains a version-incompatible opcode";return false;
            }
        }
    }
    std::vector<std::optional<FlowState>> incoming(bytecode.code.size());
    FlowState initial;initial.locals.resize(bytecode.locals.size());incoming[0]=initial;
    std::deque<std::size_t> work{0};bool halted=false;
    const auto enqueue=[&](std::size_t target,const FlowState& candidate,std::size_t from)->bool {
        if(target>=bytecode.code.size()){error="QVM control flow leaves the instruction stream";return false;}
        if(liveBytes(candidate)>MaxLive){error="QVM live string memory exceeds 16 MiB";return false;}
        auto& slot=incoming[target];
        if(!slot){slot=candidate;work.push_back(target);return true;}
        FlowState merged=*slot;
        if(merged.stack.size()!=candidate.stack.size()){error="QVM control-flow stack heights differ";return false;}
        for(std::size_t i=0;i<merged.stack.size();++i) {
            if(merged.stack[i].type!=candidate.stack[i].type){error="QVM stack types differ at control-flow merge";return false;}
            if(merged.stack[i].stringBytes!=candidate.stack[i].stringBytes) {
                if(from>=target){error="QVM loop carries varying string stack value";return false;}
                merged.stack[i].stringBytes=std::max(merged.stack[i].stringBytes,candidate.stack[i].stringBytes);
            }
        }
        for(std::size_t i=0;i<merged.locals.size();++i) {
            auto& left=merged.locals[i];const auto& right=candidate.locals[i];
            if(left.mayInit&&right.mayInit) {
                if(left.type!=right.type||left.mutableLocal!=right.mutableLocal) {
                    error="QVM local types or mutability differ at merge";return false;
                }
                if(left.stringBytes!=right.stringBytes) {
                    if(from>=target){error="QVM loop carries varying string local";return false;}
                    left.stringBytes=std::max(left.stringBytes,right.stringBytes);
                }
                left.definite=left.definite&&right.definite;
            } else if(left.mayInit||right.mayInit) {
                const auto initialized=left.mayInit?left:right;
                left={true,false,initialized.mutableLocal,initialized.type,initialized.stringBytes};
            } else left=LocalState{};
        }
        if(liveBytes(merged)>MaxLive){error="QVM live string memory exceeds 16 MiB";return false;}
        if(!sameState(*slot,merged)){slot=std::move(merged);work.push_back(target);}
        return true;
    };
    while(!work.empty()) {
        const auto pc=work.front();work.pop_front();FlowState state=*incoming[pc];const auto& ins=bytecode.code[pc];
        const auto pop=[&](StackValue& value){if(state.stack.empty())return false;value=state.stack.back();state.stack.pop_back();return true;};
        const auto push=[&](ValueType type,std::size_t bytes=0){state.stack.push_back({type,bytes});return liveBytes(state)<=MaxLive;};
        bool falls=true;
        switch(ins.op) {
            case OpCode::PushNumber:if(!push(ValueType::Number))goto memory_fail;break;
            case OpCode::PushString:if(!push(ValueType::String,ins.text.size()))goto memory_fail;break;
            case OpCode::PushBoolean:if(!push(ValueType::Boolean))goto memory_fail;break;
            case OpCode::LoadLocal:{
                if(ins.slot>=state.locals.size()||!state.locals[ins.slot].definite){error="QVM loads invalid or uninitialized local";return false;}
                const auto& local=state.locals[ins.slot];if(!push(local.type,local.stringBytes))goto memory_fail;break;
            }
            case OpCode::StoreLocal:case OpCode::StoreLocalMutable:{
                if(ins.slot>=state.locals.size()){error="QVM store has invalid local slot";return false;}
                StackValue value{};if(!pop(value)){error="QVM local store underflows stack";return false;}
                auto& local=state.locals[ins.slot];
                if(ins.op==OpCode::StoreLocal) {
                    if(local.mayInit){error="QVM immutable local initialized more than once";return false;}
                    local={true,true,false,value.type,value.stringBytes};
                } else {
                    if(local.mayInit&&!local.mutableLocal){error="QVM mutable store overwrites immutable local";return false;}
                    if(local.mayInit&&local.type!=value.type){error="QVM mutable local changes type";return false;}
                    local={true,true,true,value.type,value.stringBytes};
                }
                if(liveBytes(state)>MaxLive)goto memory_fail;break;
            }
            case OpCode::Add:case OpCode::Subtract:case OpCode::Multiply:case OpCode::Divide:{
                if(state.stack.size()<2){error="QVM arithmetic underflows stack";return false;}
                const auto rhs=state.stack.back(),lhs=state.stack[state.stack.size()-2];
                if(ins.op==OpCode::Add&&lhs.type==ValueType::String&&rhs.type==ValueType::String) {
                    const auto before=liveBytes(state);
                    if(lhs.stringBytes>MaxLive-rhs.stringBytes){error="QVM string concatenation exceeds memory limit";return false;}
                    const auto bytes=lhs.stringBytes+rhs.stringBytes;
                    if(bytes>MaxLive-before){error="QVM string concatenation peak exceeds memory limit";return false;}
                    StackValue temp{};pop(temp);pop(temp);if(!push(ValueType::String,bytes))goto memory_fail;break;
                }
                if(lhs.type!=ValueType::Number||rhs.type!=ValueType::Number){error="QVM arithmetic requires numbers";return false;}
                StackValue temp{};pop(temp);pop(temp);if(!push(ValueType::Number))goto memory_fail;break;
            }
            case OpCode::Negate:case OpCode::SquareRoot:case OpCode::Absolute:{
                StackValue value{};if(!pop(value)||value.type!=ValueType::Number){error="QVM unary numeric op requires number";return false;}
                if(!push(ValueType::Number))goto memory_fail;break;
            }
            case OpCode::Length:{
                StackValue value{};if(!pop(value)||value.type!=ValueType::String){error="QVM LENGTH requires string";return false;}
                if(!push(ValueType::Number))goto memory_fail;break;
            }
            case OpCode::LogicalNot:{
                StackValue value{};if(!pop(value)||value.type!=ValueType::Boolean){error="QVM NOT requires Boolean";return false;}
                if(!push(ValueType::Boolean))goto memory_fail;break;
            }
            case OpCode::Equal:case OpCode::NotEqual:{
                if(state.stack.size()<2){error="QVM equality underflows stack";return false;}
                const auto rhs=state.stack.back(),lhs=state.stack[state.stack.size()-2];
                if(lhs.type!=rhs.type){error="QVM equality type mismatch";return false;}
                StackValue temp{};pop(temp);pop(temp);if(!push(ValueType::Boolean))goto memory_fail;break;
            }
            case OpCode::Less:case OpCode::LessEqual:case OpCode::Greater:case OpCode::GreaterEqual:{
                if(state.stack.size()<2){error="QVM comparison underflows stack";return false;}
                const auto rhs=state.stack.back(),lhs=state.stack[state.stack.size()-2];
                if(lhs.type!=ValueType::Number||rhs.type!=ValueType::Number){error="QVM ordering requires numbers";return false;}
                StackValue temp{};pop(temp);pop(temp);if(!push(ValueType::Boolean))goto memory_fail;break;
            }
            case OpCode::Print:{StackValue value{};if(!pop(value)){error="QVM PRINT underflows stack";return false;}break;}
            case OpCode::Jump:falls=false;if(!enqueue(ins.slot,state,pc))return false;break;
            case OpCode::JumpIfFalse:{
                StackValue value{};if(!pop(value)||value.type!=ValueType::Boolean){error="QVM JUMP_IF_FALSE requires Boolean";return false;}
                if(!enqueue(ins.slot,state,pc))return false;break;
            }
            case OpCode::Halt:
                if(pc+1!=bytecode.code.size()||!state.stack.empty()){error="QVM HALT must be final with empty stack";return false;}
                halted=true;falls=false;break;
            default:error="QVM legacy bytecode contains an unsupported opcode";return false;
        }
        if(state.stack.size()>MaxLocals){error="QVM stack limit exceeded";return false;}
        if(falls){if(pc+1>=bytecode.code.size()){error="QVM falls through without HALT";return false;}if(!enqueue(pc+1,state,pc))return false;}
        continue;
memory_fail:
        error="QVM live string memory exceeds 16 MiB";return false;
    }
    if(!halted){error="QVM HALT is unreachable";return false;}
    return true;
}

} // namespace

bool verifyBytecode(const Bytecode& bytecode,std::string& error) {
    error.clear();
    if(bytecode.formatVersion==1||bytecode.formatVersion==2)return verifyLegacy(bytecode,error);
    if(bytecode.formatVersion!=3){error="unsupported QVM bytecode version";return false;}
    if(bytecode.code.empty()||bytecode.code.size()>MaxInstructions){error="QVM v3 instruction count is invalid";return false;}
    if(bytecode.locals.size()>MaxLocals||bytecode.functions.size()>MaxFunctions){error="QVM v3 local or function count exceeds limit";return false;}
    if(bytecode.mainEnd==0||bytecode.mainEnd>bytecode.code.size()||
       bytecode.code[bytecode.mainEnd-1].op!=OpCode::Halt){error="QVM v3 main range must end with HALT";return false;}
    std::size_t encoded=0;
    std::unordered_set<std::string> allNames;
    for(const auto& name:bytecode.locals) {
        if(name.empty()||name.size()>MaxString||!allNames.insert(name).second){error="QVM main local name is empty or duplicated";return false;}
        if(name.size()>MaxEncoded-encoded){error="QVM encoded string limit exceeded";return false;}encoded+=name.size();
    }
    std::size_t expectedEntry=bytecode.mainEnd;
    for(const auto& fn:bytecode.functions) {
        if(fn.name.empty()||fn.name.size()>MaxString||!allNames.insert("fn:"+fn.name).second){error="QVM function name is empty or duplicated";return false;}
        if(fn.entry!=expectedEntry||fn.end<=fn.entry||fn.end>bytecode.code.size()){error="QVM function ranges must be contiguous, ordered and non-empty";return false;}
        if(fn.locals.size()>MaxLocals||fn.parameterTypes.size()>fn.locals.size()){error="QVM function local/parameter count is invalid";return false;}
        if(!validType(fn.returnType)){error="QVM function return type is invalid";return false;}
        if(bytecode.code[fn.end-1].op!=OpCode::Return){error="QVM function range must end with RETURN";return false;}
        if(fn.name.size()>MaxEncoded-encoded){error="QVM encoded string limit exceeded";return false;}encoded+=fn.name.size();
        for(const auto& local:fn.locals) {
            if(local.empty()||local.size()>MaxString){error="QVM function local name is invalid";return false;}
            if(local.size()>MaxEncoded-encoded){error="QVM encoded string limit exceeded";return false;}encoded+=local.size();
        }
        expectedEntry=fn.end;
    }
    if(expectedEntry!=bytecode.code.size()){error="QVM v3 contains unassigned instruction ranges";return false;}
    for(std::size_t pc=0;pc<bytecode.code.size();++pc) {
        const auto& ins=bytecode.code[pc];
        if(!validOpcode(static_cast<std::uint8_t>(ins.op),3)){error="QVM v3 contains an unknown opcode";return false;}
        if(ins.text.size()>MaxString||ins.text.size()>MaxEncoded-encoded){error="QVM v3 text operands exceed limit";return false;}
        encoded+=ins.text.size();
        if(ins.op==OpCode::PushNumber&&!std::isfinite(ins.number)){error="QVM v3 contains a non-finite number";return false;}
        if(ins.op==OpCode::PushBoolean&&ins.slot>1){error="QVM v3 Boolean constant must be 0 or 1";return false;}
        if((ins.op==OpCode::Jump||ins.op==OpCode::JumpIfFalse)&&ins.slot>=bytecode.code.size()){error="QVM v3 jump target is out of range";return false;}
        if(ins.op==OpCode::Call&&ins.slot>=bytecode.functions.size()){error="QVM CALL target is out of range";return false;}
        if(ins.op==OpCode::Call&&ins.argumentCount!=bytecode.functions[ins.slot].parameterTypes.size()){
            error="QVM CALL argument count does not match function signature";return false;
        }
    }
    if(!verifySegment(bytecode,0,bytecode.mainEnd,bytecode.locals,{},nullptr,true,error))return false;
    for(const auto& fn:bytecode.functions)
        if(!verifySegment(bytecode,fn.entry,fn.end,fn.locals,fn.parameterTypes,&fn.returnType,false,error))return false;
    return true;
}

bool Bytecode::save(std::ostream& out,std::string& error)const {
    if(!verifyBytecode(*this,error))return false;
    const std::array<char,8> v1={'T','V','Q','V','M','0','1','\0'};
    const std::array<char,8> v2={'T','V','Q','V','M','0','2','\0'};
    const std::array<char,8> v3={'T','V','Q','V','M','0','3','\0'};
    const auto& magic=formatVersion==1?v1:(formatVersion==2?v2:v3);
    out.write(magic.data(),8);
    putU32(out,static_cast<std::uint32_t>(code.size()));
    putU32(out,static_cast<std::uint32_t>(locals.size()));
    if(formatVersion==3) {
        putU32(out,static_cast<std::uint32_t>(functions.size()));
        putU32(out,mainEnd);
    }
    for(const auto& local:locals)if(!putString(out,local)){error="failed to write QVM main local name";return false;}
    if(formatVersion==3) {
        for(const auto& fn:functions) {
            if(!putString(out,fn.name)){error="failed to write QVM function name";return false;}
            putU32(out,fn.entry);putU32(out,fn.end);
            out.put(static_cast<char>(fn.returnType));
            putU32(out,static_cast<std::uint32_t>(fn.parameterTypes.size()));
            for(const auto type:fn.parameterTypes)out.put(static_cast<char>(type));
            putU32(out,static_cast<std::uint32_t>(fn.locals.size()));
            for(const auto& local:fn.locals)if(!putString(out,local)){error="failed to write QVM function local";return false;}
        }
    }
    for(const auto& ins:code) {
        out.put(static_cast<char>(ins.op));putU32(out,ins.slot);
        if(formatVersion==3)putU32(out,ins.argumentCount);
        putU64(out,std::bit_cast<std::uint64_t>(ins.number));
        if(!putString(out,ins.text)){error="failed to write QVM instruction text";return false;}
    }
    if(!out){error="failed to write QVM bytecode";return false;}
    return true;
}

bool Bytecode::load(std::istream& in,Bytecode& out,std::string& error) {
    std::array<char,8> magic{};
    if(!in.read(magic.data(),8)){error="truncated QVM header";return false;}
    const std::array<char,8> v1={'T','V','Q','V','M','0','1','\0'};
    const std::array<char,8> v2={'T','V','Q','V','M','0','2','\0'};
    const std::array<char,8> v3={'T','V','Q','V','M','0','3','\0'};
    if(magic!=v1&&magic!=v2&&magic!=v3){error="invalid QVM magic or unsupported version";return false;}
    Bytecode temp;temp.formatVersion=magic==v1?1u:(magic==v2?2u:3u);
    std::uint32_t count{},localCount{},functionCount{};
    if(!getU32(in,count)||!getU32(in,localCount)||count>MaxInstructions||localCount>MaxLocals){
        error="invalid QVM section sizes";return false;
    }
    if(temp.formatVersion==3) {
        if(!getU32(in,functionCount)||!getU32(in,temp.mainEnd)||functionCount>MaxFunctions){
            error="invalid QVM v3 metadata sizes";return false;
        }
    }
    std::size_t encoded=0;
    for(std::uint32_t i=0;i<localCount;++i) {
        std::string local;if(!getString(in,local,encoded)){error="invalid QVM main local table";return false;}
        temp.locals.push_back(std::move(local));
    }
    for(std::uint32_t i=0;i<functionCount;++i) {
        FunctionInfo fn;
        if(!getString(in,fn.name,encoded)||!getU32(in,fn.entry)||!getU32(in,fn.end)){
            error="invalid QVM function metadata";return false;
        }
        const int returnType=in.get();
        std::uint32_t parameterCount{},fnLocalCount{};
        if(returnType<1||returnType>3||!getU32(in,parameterCount)||parameterCount>MaxLocals){
            error="invalid QVM function signature";return false;
        }
        fn.returnType=static_cast<ValueType>(returnType);
        for(std::uint32_t p=0;p<parameterCount;++p) {
            const int type=in.get();
            if(type<1||type>3){error="invalid QVM parameter type";return false;}
            fn.parameterTypes.push_back(static_cast<ValueType>(type));
        }
        if(!getU32(in,fnLocalCount)||fnLocalCount>MaxLocals){error="invalid QVM function local count";return false;}
        for(std::uint32_t n=0;n<fnLocalCount;++n) {
            std::string local;
            if(!getString(in,local,encoded)){error="invalid QVM function local table";return false;}
            fn.locals.push_back(std::move(local));
        }
        temp.functions.push_back(std::move(fn));
    }
    for(std::uint32_t i=0;i<count;++i) {
        const int raw=in.get();std::uint32_t slot{},argumentCount=0;
        if(raw<1||!validOpcode(static_cast<std::uint8_t>(raw),temp.formatVersion)||!getU32(in,slot)){
            error="invalid or version-incompatible QVM instruction";return false;
        }
        if(temp.formatVersion==3&&!getU32(in,argumentCount)){error="truncated QVM call metadata";return false;}
        std::uint64_t bits{};
        if(!getU64(in,bits)){error="truncated QVM numeric operand";return false;}
        std::string text;if(!getString(in,text,encoded)){error="invalid QVM instruction text";return false;}
        temp.code.push_back({static_cast<OpCode>(raw),std::bit_cast<double>(bits),slot,std::move(text),argumentCount});
    }
    if(in.peek()!=EOF){error="QVM file contains trailing data after final instruction";return false;}
    if(!verifyBytecode(temp,error))return false;
    out=std::move(temp);return true;
}

std::string disassemble(const Bytecode& bytecode) {
    std::string output;
    for(std::size_t pc=0;pc<bytecode.code.size();++pc) {
        const auto& ins=bytecode.code[pc];
        output+=std::to_string(pc)+"  "+name(ins.op);
        if(ins.op==OpCode::PushNumber)output+=" "+std::to_string(ins.number);
        if(ins.op==OpCode::PushString)output+=" \""+ins.text+"\"";
        if(ins.op==OpCode::LoadLocal||ins.op==OpCode::StoreLocal||ins.op==OpCode::StoreLocalMutable||
           ins.op==OpCode::Jump||ins.op==OpCode::JumpIfFalse||ins.op==OpCode::Call)
            output+=" "+std::to_string(ins.slot);
        if(ins.op==OpCode::Call)output+=" argc="+std::to_string(ins.argumentCount);
        if(ins.op==OpCode::PushBoolean)output+=ins.slot?" true":" false";
        output+='\n';
    }
    return output;
}
}
