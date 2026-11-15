#pragma once
#include "taviq/ast.hpp"
#include "taviq/bytecode.hpp"
#include <string>
#include <vector>
namespace taviq {
bool compile(const Program& program, Bytecode& output, std::vector<std::string>& errors);
bool emitC(const Program& program, std::string& output, std::vector<std::string>& errors);
}