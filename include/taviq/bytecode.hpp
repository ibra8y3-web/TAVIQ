#pragma once
#include <cstdint>
#include <iosfwd>
#include <string>
#include <vector>
namespace taviq {
enum class OpCode : std::uint8_t { PushNumber=1, PushString=2, LoadLocal=3, StoreLocal=4, Add=5, Subtract=6, Multiply=7, Divide=8, Negate=9, Print=10, Halt=11 };
struct Instruction { OpCode op{OpCode::Halt}; double number{}; std::uint32_t slot{}; std::string text; };
struct Bytecode { std::vector<Instruction> code; std::vector<std::string> locals; bool save(std::ostream& out, std::string& error) const; static bool load(std::istream& in, Bytecode& out, std::string& error); };
std::string disassemble(const Bytecode& bytecode);
}