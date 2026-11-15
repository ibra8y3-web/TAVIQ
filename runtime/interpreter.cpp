#include "taviq/interpreter.hpp"
#include <iostream>

namespace taviq {
int Interpreter::run(const Program& program) const {
    for (const auto& statement : program.statements)
        std::cout << statement.text << '\n';
    return 0;
}
}
