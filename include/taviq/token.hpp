#pragma once
#include <string>

namespace taviq {
enum class TokenKind {
    Flow, Main, Display, LeftBrace, RightBrace, LeftParen, RightParen,
    Semicolon, String, End
};
struct Token {
    TokenKind kind{};
    std::string lexeme;
    std::size_t line{1};
};
}
