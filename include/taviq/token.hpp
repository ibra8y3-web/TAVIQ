#pragma once
#include <cstddef>
#include <string>
namespace taviq {
enum class TokenKind {
    Flow, Main, Let, Var, Display, If, Else, While, True, False, And, Or, Not,
    Identifier, Number, String,
    LeftBrace, RightBrace, LeftParen, RightParen, Semicolon, Comma, Colon,
    Equal, EqualEqual, Bang, BangEqual, Less, LessEqual, Greater, GreaterEqual,
    Plus, Minus, Star, Slash, End
};
struct Token { TokenKind kind{}; std::string lexeme; std::size_t line{1}; };
}
