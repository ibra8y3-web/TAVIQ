#include "taviq/bytecode.hpp"
#include "taviq/compiler.hpp"
#include "taviq/compute.hpp"
#include "taviq/lexer.hpp"
#include "taviq/native.hpp"
#include "taviq/parser.hpp"
#include "taviq/vm.hpp"
#include <cassert>
#include <sstream>
#include <string>
#include <vector>

static taviq::Program parse(const std::string& source) {
    taviq::Lexer lexer(source); auto tokens=lexer.scan();
    assert(lexer.errors().empty());
    taviq::Parser parser(tokens); auto program=parser.parse();
    assert(parser.errors().empty());
    return program;
}
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
    taviq::Bytecode code; std::vector<std::string> errors;
    assert(taviq::compile(program,code,errors)); assert(errors.empty());
    std::ostringstream output; std::string error;
    assert(taviq::VM{}.run(code,output,error)==0);
    assert(output.str()=="14\n25\nhello TAVIQ\n");

    std::stringstream bytes(std::ios::in|std::ios::out|std::ios::binary);
    assert(code.save(bytes,error)); bytes.seekg(0);
    taviq::Bytecode restored;
    assert(taviq::Bytecode::load(bytes,restored,error));
    std::ostringstream restoredOutput;
    assert(taviq::VM{}.run(restored,restoredOutput,error)==0);
    assert(restoredOutput.str()==output.str());
    assert(taviq::disassemble(restored).find("PUSH_NUMBER")!=std::string::npos);

    auto bad=parse("flow main { display unknown; }");
    taviq::Bytecode invalid;
    assert(!taviq::compile(bad,invalid,errors)); errors.clear();

    auto division=parse("flow main { display 5 / 0; }");
    taviq::Bytecode divCode; assert(taviq::compile(division,divCode,errors));
    std::ostringstream ignored; assert(taviq::VM{}.run(divCode,ignored,error)!=0);
    assert(error=="division by zero");

    auto nativeProgram=parse("flow main { let answer = 6 * 7; display answer; display \"native-ready\"; }");
    std::string cSource;
    assert(taviq::emitC(nativeProgram,cSource,errors));
    assert(cSource.find("int main(void)")!=std::string::npos);
    assert(cSource.find("native-ready")!=std::string::npos);

    std::vector<int> values(1000,0);
    taviq::parallelFor(0,values.size(),13,[&](std::size_t i){values[i]=static_cast<int>(i*i);},4);
    for(std::size_t i=0;i<values.size();++i)assert(values[i]==static_cast<int>(i*i));
    return 0;
}
