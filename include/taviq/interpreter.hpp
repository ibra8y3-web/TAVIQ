#pragma once
#include "taviq/ast.hpp"

namespace taviq {
class Interpreter {
public:
    int run(const Program& program) const;
};
}
