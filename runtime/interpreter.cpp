#include "taviq/interpreter.hpp"
#include "taviq/compiler.hpp"
#include "taviq/vm.hpp"
#include <iostream>
namespace taviq {
int Interpreter::run(const Program& program) const {
 Bytecode bytecode;std::vector<std::string> errors;
 if(!compile(program,bytecode,errors)){for(const auto& e:errors)std::cerr<<e<<'\n';return 1;}
 std::string error;return VM{}.run(bytecode,std::cout,error)?(std::cerr<<error<<'\n',1):0;
}
}