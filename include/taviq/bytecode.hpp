#pragma once
#include <cstdint>
#include <iosfwd>
#include <string>
#include <vector>
namespace taviq {
enum class ValueType : std::uint8_t { Number=1, String=2, Boolean=3 };
enum class OpCode : std::uint8_t {
    PushNumber=1, PushString=2, LoadLocal=3, StoreLocal=4, Add=5,
    Subtract=6, Multiply=7, Divide=8, Negate=9, Print=10, Halt=11,
    Length=12, SquareRoot=13, Absolute=14,
    PushBoolean=15, Equal=16, NotEqual=17, Less=18, LessEqual=19,
    Greater=20, GreaterEqual=21, LogicalNot=22, Jump=23,
    JumpIfFalse=24, StoreLocalMutable=25,
    Call=26, Return=27
};
struct Instruction {
    OpCode op{OpCode::Halt};
    double number{};
    /* Local slot, Boolean payload (0/1), jump target, or function index. */
    std::uint32_t slot{};
    std::string text;
    std::uint32_t argumentCount{};
};
struct FunctionInfo {
    std::string name;
    std::uint32_t entry{};
    std::uint32_t end{};
    std::vector<std::string> locals;
    std::vector<ValueType> parameterTypes;
    ValueType returnType{ValueType::Number};
};
struct Bytecode {
    std::uint32_t formatVersion{3};
    std::uint32_t mainEnd{};
    std::vector<Instruction> code;
    std::vector<std::string> locals;
    std::vector<FunctionInfo> functions;
    bool save(std::ostream& out,std::string& error) const;
    static bool load(std::istream& in,Bytecode& out,std::string& error);
};
bool verifyBytecode(const Bytecode& bytecode,std::string& error);
std::string disassemble(const Bytecode& bytecode);
}
