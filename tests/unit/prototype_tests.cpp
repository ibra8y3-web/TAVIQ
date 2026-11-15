#include "taviq/lexer.hpp"
#include <cassert>

int main() {
    taviq::Lexer lexer("flow main { display \"ok\"; }");
    const auto tokens = lexer.scan();
    assert(lexer.errors().empty());
    assert(tokens.size() == 8); // flow, main, {, display, string, ;, }, EOF
    return 0;
}
