#pragma once
#include <cstddef>
#include <string>
namespace taviq {
enum class TokenKind { Flow, Main, Let, Display, Identifier, Number, String, LeftBrace, RightBrace, LeftParen, RightParen, Semicolon, Equal, Plus, Minus, Star, Slash, End };
struct Token { TokenKind kind{}; std::string lexeme; std::size_t line{1}; };
}