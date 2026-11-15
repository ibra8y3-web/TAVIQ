#pragma once
#include "taviq/ast.hpp"
#include <string>
#include <vector>
namespace taviq { bool emitC(const Program& program,std::string& output,std::vector<std::string>& errors); }