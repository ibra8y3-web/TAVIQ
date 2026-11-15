#include "taviq/bytecode.hpp"
#include "taviq/compiler.hpp"
#include "taviq/compute.hpp"
#include "taviq/lexer.hpp"
#include "taviq/native.hpp"
#include "taviq/parser.hpp"
#include "taviq/vm.hpp"
#include <atomic>
#include <iostream>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

static taviq::Program parse(const std::string& source) {
    taviq::Lexer lexer(source);
    auto tokens=lexer.scan();
    if (!lexer.errors().empty()) throw std::runtime_error(lexer.errors().front());
    taviq::Parser parser(tokens);
    auto program=parser.parse();
    if (!parser.errors().empty()) throw std::runtime_error(parser.errors().front());
    return program;
}

#define CHECK(condition) do { if (!(condition)) { \
    std::cerr << "CHECK failed at " << __FILE__ << ":" << __LINE__ << ": " #condition "\n"; \
    return 1; \
} } while (false)

int main() {
    const std::string source =
        "flow main {\n"
        "  let base = 2 + 3 * 4;\n"
        "  let total = (base - 4) / 2;\n"
        "  display base;\n"
        "  display total * total;\n"
        "  display \"hello \" + \"TAVIQ\";\n"
        "}\n";
    auto program=parse(source);
    taviq::Bytecode code; std::vector<std::string> errors; std::string error;
    CHECK(taviq::compile(program,code,errors));
    CHECK(errors.empty());
    CHECK(taviq::verifyBytecode(code,error));
    std::ostringstream output;
    CHECK(taviq::VM{}.run(code,output,error)==0);
    CHECK(output.str()=="14\n25\nhello TAVIQ\n");

    std::stringstream bytes(std::ios::in|std::ios::out|std::ios::binary);
    CHECK(code.save(bytes,error));
    bytes.seekg(0);
    taviq::Bytecode restored;
    CHECK(taviq::Bytecode::load(bytes,restored,error));
    std::ostringstream restoredOutput;
    CHECK(taviq::VM{}.run(restored,restoredOutput,error)==0);
    CHECK(restoredOutput.str()==output.str());
    CHECK(taviq::disassemble(restored).find("PUSH_NUMBER")!=std::string::npos);

    auto bad=parse("flow main { display unknown; }");
    taviq::Bytecode invalid;
    CHECK(!taviq::compile(bad,invalid,errors));
    errors.clear();

    auto division=parse("flow main { display 5 / 0; }");
    taviq::Bytecode divCode;
    CHECK(taviq::compile(division,divCode,errors));
    std::ostringstream ignored;
    CHECK(taviq::VM{}.run(divCode,ignored,error)!=0);
    CHECK(error=="division by zero");

    taviq::Bytecode noHalt;
    noHalt.code.push_back({taviq::OpCode::PushNumber,1.0,0,{}});
    CHECK(!taviq::verifyBytecode(noHalt,error));
    CHECK(error.find("HALT")!=std::string::npos);
    taviq::Bytecode underflow;
    underflow.code.push_back({taviq::OpCode::Print,0,0,{}});
    underflow.code.push_back({taviq::OpCode::Halt,0,0,{}});
    CHECK(!taviq::verifyBytecode(underflow,error));
    taviq::Bytecode badLocal;
    badLocal.locals={"x"};
    badLocal.code.push_back({taviq::OpCode::LoadLocal,0,0,{}});
    badLocal.code.push_back({taviq::OpCode::Print,0,0,{}});
    badLocal.code.push_back({taviq::OpCode::Halt,0,0,{}});
    CHECK(!taviq::verifyBytecode(badLocal,error));
    taviq::Bytecode badTypes;
    badTypes.code.push_back({taviq::OpCode::PushNumber,1.0,0,{}});
    badTypes.code.push_back({taviq::OpCode::PushString,0,0,"x"});
    badTypes.code.push_back({taviq::OpCode::Add,0,0,{}});
    badTypes.code.push_back({taviq::OpCode::Halt,0,0,{}});
    CHECK(!taviq::verifyBytecode(badTypes,error));
    taviq::Bytecode trailing;
    trailing.code.push_back({taviq::OpCode::Halt,0,0,{}});
    trailing.code.push_back({taviq::OpCode::PushNumber,1.0,0,{}});
    CHECK(!taviq::verifyBytecode(trailing,error));
    taviq::Bytecode badOpcode;
    badOpcode.code.push_back({static_cast<taviq::OpCode>(255),0,0,{}});
    CHECK(!taviq::verifyBytecode(badOpcode,error));
    std::stringstream rejected(std::ios::in|std::ios::out|std::ios::binary);
    CHECK(!badOpcode.save(rejected,error));
    CHECK(taviq::VM{}.run(badOpcode,ignored,error)!=0);

    auto nativeProgram=parse("flow main { let answer = 6 * 7; display answer; display \"native-ready\"; }");
    std::string cSource;
    CHECK(taviq::emitC(nativeProgram,cSource,errors));
    CHECK(cSource.find("int main(void)")!=std::string::npos);
    CHECK(cSource.find("native-ready")!=std::string::npos);

    std::vector<int> values(1000,0);
    taviq::parallelFor(0,values.size(),13,[&](std::size_t i){values[i]=static_cast<int>(i*i);},4);
    for(std::size_t i=0;i<values.size();++i)CHECK(values[i]==static_cast<int>(i*i));
    std::atomic<std::size_t> calls{0};
    taviq::parallelFor(0,0,0,[&](std::size_t){++calls;},1);
    CHECK(calls.load()==0);
    taviq::parallelFor(std::numeric_limits<std::size_t>::max()-1,
                       std::numeric_limits<std::size_t>::max(),5,
                       [&](std::size_t i){if(i==std::numeric_limits<std::size_t>::max()-1)++calls;},1);
    CHECK(calls.load()==1);
    bool propagated=false;
    try {
        taviq::parallelFor(0,64,1,[](std::size_t i){if(i==7)throw std::runtime_error("worker failure");},4);
    } catch (const std::runtime_error&) { propagated=true; }
    CHECK(propagated);
    return 0;
}
