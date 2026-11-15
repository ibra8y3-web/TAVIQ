#pragma once
#include <string>
#include <vector>

namespace taviq {
struct Statement { std::string text; std::size_t line{}; };
struct Program { std::vector<Statement> statements; };
}
