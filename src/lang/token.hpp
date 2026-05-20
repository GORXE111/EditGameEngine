#pragma once
#include <string>
#include <cstdint>

namespace farm::lang {

// FarmCode v1 token kinds. Syntax is Lua-flavoured: keyword-delimited blocks
// (`then/do/...end`), `#` line comments, only int and bool values.
enum class Tok {
    // literals / names
    Int, Ident,
    // keywords
    KwIf, KwThen, KwElse, KwEnd, KwWhile, KwDo, KwRepeat,
    KwFunc, KwReturn, KwTrue, KwFalse, KwAnd, KwOr, KwNot,
    // punctuation
    LParen, RParen, LBracket, RBracket, Comma,
    // operators
    Assign,                       // =
    Eq, Ne, Lt, Le, Gt, Ge,      // == != < <= > >=
    Plus, Minus, Star, Slash, Percent,
    // control
    Eof,
};

struct Token {
    Tok kind;
    std::string text;   // lexeme (identifiers/keywords) — empty otherwise
    int64_t ivalue = 0; // for Tok::Int
    int line = 1;
    int col = 1;
};

const char* tok_name(Tok k);

}  // namespace farm::lang
