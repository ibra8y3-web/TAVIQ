#pragma once
#include <cstdint>
#include <string>
#include <vector>
namespace taviq::ir {
using ValueId=std::uint32_t;
enum class Type:std::uint8_t{Number,Boolean};
enum class Op:std::uint8_t{ConstantNumber,ConstantBoolean,Add,Subtract,Multiply,Divide,Negate,Equal,Less,LogicalNot};
struct Instruction{
 Op op{Op::ConstantNumber};ValueId result{};Type type{Type::Number};
 ValueId left{},right{};double number{};bool boolean{};
};
enum class TerminatorKind:std::uint8_t{Return,Jump,Branch,Unreachable};
struct Terminator{TerminatorKind kind{TerminatorKind::Unreachable};ValueId value{};std::uint32_t first{},second{};};
struct Block{std::uint32_t id{};std::vector<Instruction> instructions;Terminator terminator;};
struct Function{std::string name;Type returnType{Type::Number};std::vector<Block> blocks;};
struct Module{std::vector<Function> functions;};
bool verify(const Module& module,std::string& error);
void optimize(Module& module);
std::string dump(const Module& module);
}
