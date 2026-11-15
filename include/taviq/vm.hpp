#pragma once
#include "taviq/bytecode.hpp"
#include <cstddef>
#include <functional>
#include <iosfwd>
#include <string>
namespace taviq {
class VM {
public:
    using TraceCallback = std::function<void(std::size_t,const Instruction&)>;
    int run(const Bytecode& bytecode, std::ostream& output, std::string& error,
            std::size_t instructionBudget=1000000,const TraceCallback& trace={}) const;
};
}