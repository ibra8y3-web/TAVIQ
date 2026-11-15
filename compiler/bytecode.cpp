#include "taviq/bytecode.hpp"
#include <array>
#include <bit>
#include <cmath>
#include <cstdio>
#include <istream>
#include <ostream>
#include <unordered_set>
#include <utility>
namespace taviq {
namespace {
constexpr std::size_t MaxInstructions = 1'000'000;
constexpr std::size_t MaxLocals = 65'536;
constexpr std::size_t MaxString = 1 * 1024 * 1024;
constexpr std::size_t MaxEncodedStrings = 16 * 1024 * 1024;
constexpr std::size_t MaxLiveStringBytes = 16 * 1024 * 1024;

void putU32(std::ostream& out, std::uint32_t value) {
    for (int i=0; i<4; ++i) out.put(static_cast<char>((value >> (i*8)) & 0xff));
}
bool getU32(std::istream& in, std::uint32_t& value) {
    value=0;
    for (int i=0; i<4; ++i) {
        const int c=in.get();
        if (c==EOF) return false;
        value |= static_cast<std::uint32_t>(static_cast<unsigned char>(c)) << (i*8);
    }
    return true;
}
bool putString(std::ostream& out, const std::string& value) {
    if (value.size()>MaxString) return false;
    putU32(out,static_cast<std::uint32_t>(value.size()));
    out.write(value.data(),static_cast<std::streamsize>(value.size()));
    return static_cast<bool>(out);
}
bool getString(std::istream& in, std::string& value, std::size_t& total) {
    std::uint32_t size{};
    if (!getU32(in,size) || size>MaxString || total>MaxEncodedStrings-size) return false;
    value.resize(size);
    if (!in.read(value.data(),static_cast<std::streamsize>(size))) return false;
    total+=size;
    return true;
}
const char* name(OpCode op) {
    switch (op) {
        case OpCode::PushNumber: return "PUSH_NUMBER";
        case OpCode::PushString: return "PUSH_STRING";
        case OpCode::LoadLocal: return "LOAD_LOCAL";
        case OpCode::StoreLocal: return "STORE_LOCAL";
        case OpCode::Add: return "ADD";
        case OpCode::Subtract: return "SUB";
        case OpCode::Multiply: return "MUL";
        case OpCode::Divide: return "DIV";
        case OpCode::Negate: return "NEG";
        case OpCode::Print: return "PRINT";
        case OpCode::Halt: return "HALT";
        case OpCode::Length: return "LENGTH";
        case OpCode::SquareRoot: return "SQRT";
        case OpCode::Absolute: return "ABS";
    }
    return "INVALID";
}
}

bool verifyBytecode(const Bytecode& bytecode, std::string& error) {
    error.clear();
    const auto fail=[&](const std::string& message) { error=message; return false; };
    if (bytecode.code.empty() || bytecode.code.size()>MaxInstructions)
        return fail("QVM instruction count is empty or exceeds the v1 limit");
    if (bytecode.locals.size()>MaxLocals)
        return fail("QVM local table exceeds the v1 limit");

    std::unordered_set<std::string> names;
    std::size_t encodedBytes=0;
    for (const auto& local : bytecode.locals) {
        if (local.empty() || local.size()>MaxString || !names.insert(local).second)
            return fail("QVM local names must be non-empty, bounded and unique");
        if (local.size()>MaxEncodedStrings-encodedBytes)
            return fail("QVM local-name table exceeds the v1 limit");
        encodedBytes+=local.size();
    }

    enum class ValueType : std::uint8_t { Number, String };
    struct StackValue { ValueType type; std::size_t stringBytes; };
    std::vector<int> localTypes(bytecode.locals.size(),-1);
    std::vector<std::size_t> localStringBytes(bytecode.locals.size(),0);
    std::vector<StackValue> stack;
    std::size_t liveStringBytes=0;
    bool halted=false;

    const auto push=[&](ValueType type,std::size_t bytes) -> bool {
        if (bytes>MaxLiveStringBytes-liveStringBytes) return false;
        liveStringBytes+=bytes;
        stack.push_back({type,bytes});
        return true;
    };
    const auto pop=[&](StackValue& value) -> bool {
        if (stack.empty()) return false;
        value=stack.back();
        stack.pop_back();
        liveStringBytes-=value.stringBytes;
        return true;
    };

    for (std::size_t pc=0; pc<bytecode.code.size(); ++pc) {
        const auto& ins=bytecode.code[pc];
        if (ins.text.size()>MaxString || ins.text.size()>MaxEncodedStrings-encodedBytes)
            return fail("QVM string operands exceed the v1 limit");
        encodedBytes+=ins.text.size();

        switch (ins.op) {
            case OpCode::PushNumber:
                if (!std::isfinite(ins.number)) return fail("QVM contains a non-finite numeric constant");
                if (!push(ValueType::Number,0)) return fail("QVM live string memory exceeds the v1 limit");
                break;
            case OpCode::PushString:
                if (!push(ValueType::String,ins.text.size())) return fail("QVM live string memory exceeds the v1 limit");
                break;
            case OpCode::LoadLocal:
                if (ins.slot>=localTypes.size() || localTypes[ins.slot]<0)
                    return fail("QVM loads an invalid or uninitialized local");
                if (!push(static_cast<ValueType>(localTypes[ins.slot]),localStringBytes[ins.slot]))
                    return fail("QVM live string memory exceeds the v1 limit");
                break;
            case OpCode::StoreLocal: {
                StackValue value{};
                if (ins.slot>=localTypes.size() || localTypes[ins.slot]>=0 || !pop(value))
                    return fail("QVM local store is invalid, duplicated or underflows the stack");
                localTypes[ins.slot]=static_cast<int>(value.type);
                localStringBytes[ins.slot]=value.stringBytes;
                if (value.stringBytes>MaxLiveStringBytes-liveStringBytes)
                    return fail("QVM local memory exceeds the v1 limit");
                liveStringBytes+=value.stringBytes;
                break;
            }
            case OpCode::Add:
            case OpCode::Subtract:
            case OpCode::Multiply:
            case OpCode::Divide: {
                if (stack.size()<2) return fail("QVM arithmetic instruction underflows the stack");
                const auto rhs=stack.back();
                const auto lhs=stack[stack.size()-2];
                if (ins.op==OpCode::Add && lhs.type==ValueType::String && rhs.type==ValueType::String) {
                    if (lhs.stringBytes>MaxLiveStringBytes-rhs.stringBytes)
                        return fail("QVM string concatenation exceeds the v1 limit");
                    const auto resultBytes=lhs.stringBytes+rhs.stringBytes;
                    if (resultBytes>MaxLiveStringBytes-liveStringBytes)
                        return fail("QVM string concatenation peak memory exceeds the v1 limit");
                    StackValue discarded{};
                    pop(discarded); pop(discarded);
                    if (!push(ValueType::String,resultBytes))
                        return fail("QVM live string memory exceeds the v1 limit");
                    break;
                }
                StackValue discarded{};
                pop(discarded); pop(discarded);
                if (lhs.type!=ValueType::Number || rhs.type!=ValueType::Number)
                    return fail("QVM arithmetic operands have incompatible types");
                if (!push(ValueType::Number,0)) return fail("QVM live string memory exceeds the v1 limit");
                break;
            }
            case OpCode::Negate:
            case OpCode::SquareRoot:
            case OpCode::Absolute: {
                StackValue value{};
                if (!pop(value) || value.type!=ValueType::Number)
                    return fail("QVM numeric unary operator requires one number");
                if (!push(ValueType::Number,0)) return fail("QVM live string memory exceeds the v1 limit");
                break;
            }
            case OpCode::Length: {
                StackValue value{};
                if (!pop(value) || value.type!=ValueType::String)
                    return fail("QVM LENGTH requires one string");
                if (!push(ValueType::Number,0)) return fail("QVM live string memory exceeds the v1 limit");
                break;
            }
            case OpCode::Print: {
                StackValue value{};
                if (!pop(value)) return fail("QVM PRINT underflows the stack");
                break;
            }
            case OpCode::Halt:
                if (pc+1!=bytecode.code.size()) return fail("QVM HALT must be the final instruction");
                if (!stack.empty()) return fail("QVM HALT requires an empty operand stack");
                halted=true;
                break;
            default:
                return fail("QVM contains an unknown opcode");
        }
        if (stack.size()>MaxLocals) return fail("QVM operand stack exceeds the v1 limit");
    }
    if (!halted) return fail("QVM bytecode has no final HALT instruction");
    return true;
}

bool Bytecode::save(std::ostream& out,std::string& error) const {
    if (!verifyBytecode(*this,error)) return false;
    const std::array<char,8> magic={'T','V','Q','V','M','0','1','\0'};
    out.write(magic.data(),8);
    putU32(out,static_cast<std::uint32_t>(code.size()));
    putU32(out,static_cast<std::uint32_t>(locals.size()));
    for (const auto& local : locals) {
        if (!putString(out,local)) { error="failed to write QVM local table"; return false; }
    }
    for (const auto& ins : code) {
        out.put(static_cast<char>(ins.op));
        putU32(out,ins.slot);
        const auto bits=std::bit_cast<std::uint64_t>(ins.number);
        for (int n=0; n<8; ++n) out.put(static_cast<char>((bits>>(n*8))&0xff));
        if (!putString(out,ins.text)) { error="failed to write QVM operand"; return false; }
    }
    if (!out) { error="failed to write bytecode"; return false; }
    return true;
}

bool Bytecode::load(std::istream& in,Bytecode& out,std::string& error) {
    std::array<char,8> magic{};
    if (!in.read(magic.data(),8) || magic!=std::array<char,8>{'T','V','Q','V','M','0','1','\0'}) {
        error="invalid QVM magic or version";
        return false;
    }
    std::uint32_t count{},localCount{};
    if (!getU32(in,count) || !getU32(in,localCount) || count>MaxInstructions || localCount>MaxLocals) {
        error="invalid QVM section sizes";
        return false;
    }
    Bytecode temporary;
    std::size_t encodedBytes=0;
    for (std::uint32_t n=0; n<localCount; ++n) {
        std::string local;
        if (!getString(in,local,encodedBytes)) { error="invalid QVM local table"; return false; }
        temporary.locals.push_back(std::move(local));
    }
    for (std::uint32_t n=0; n<count; ++n) {
        const int raw=in.get();
        std::uint32_t slot{};
        if (raw<1 || raw>14 || !getU32(in,slot)) { error="invalid QVM instruction"; return false; }
        std::uint64_t bits=0;
        for (int k=0; k<8; ++k) {
            const int c=in.get();
            if (c==EOF) { error="truncated QVM numeric operand"; return false; }
            bits|=static_cast<std::uint64_t>(static_cast<unsigned char>(c))<<(k*8);
        }
        std::string text;
        if (!getString(in,text,encodedBytes)) { error="invalid QVM string operand"; return false; }
        temporary.code.push_back({static_cast<OpCode>(raw),std::bit_cast<double>(bits),slot,std::move(text)});
    }
    if (in.peek()!=EOF) { error="QVM file contains trailing data after the final instruction"; return false; }
    if (!verifyBytecode(temporary,error)) return false;
    out=std::move(temporary);
    return true;
}

std::string disassemble(const Bytecode& bytecode) {
    std::string output;
    for (std::size_t i=0; i<bytecode.code.size(); ++i) {
        const auto& ins=bytecode.code[i];
        output+=std::to_string(i)+"  "+name(ins.op);
        if (ins.op==OpCode::PushNumber) output+=" "+std::to_string(ins.number);
        if (ins.op==OpCode::PushString) output+=" \""+ins.text+"\"";
        if (ins.op==OpCode::LoadLocal || ins.op==OpCode::StoreLocal)
            output+=" "+std::to_string(ins.slot);
        output+='\n';
    }
    return output;
}
}
