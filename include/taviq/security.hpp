#pragma once
#include <cstddef>
namespace taviq {
struct RuntimeLimits { std::size_t maxInstructions{1000000}; std::size_t maxStack{65536}; std::size_t maxLocals{65536}; };
}