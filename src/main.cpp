#include "taviq/lexer.hpp"
#include "taviq/parser.hpp"
#include "taviq/interpreter.hpp"
#include <fstream>
#include <iostream>
#include <sstream>

int main(int argc, char** argv) {
    if (argc != 2) {
        std::cerr << "Usage: taviq <file.taviq>\n";
        return 2;
    }
    std::ifstream input(argv[1], std::ios::binary);
    if (!input) {
        std::cerr << "TAVIQ: cannot open source file: " << argv[1] << '\n';
        return 2;
    }
    std::ostringstream buffer;
    buffer << input.rdbuf();

    taviq::Lexer lexer(buffer.str());
    auto tokens = lexer.scan();
    if (!lexer.errors().empty()) {
        for (const auto& e : lexer.errors()) std::cerr << e << '\n';
        return 1;
    }

    taviq::Parser parser(tokens);
    auto program = parser.parse();
    if (!parser.errors().empty()) {
        for (const auto& e : parser.errors()) std::cerr << e << '\n';
        return 1;
    }

    taviq::Interpreter interpreter;
    return interpreter.run(program);
}
