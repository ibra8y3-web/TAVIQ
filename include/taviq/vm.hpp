#pragma once
#include "taviq/bytecode.hpp"
#include <cstddef>
#include <iosfwd>
#include <string>
namespace taviq {
class VM {
public:
    int run(const Bytecode& bytecode, std::ostream& output, std::string& error, std::size_t instructionBudget=1000000) const;
};
}