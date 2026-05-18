#pragma once
#include <stdexcept>
#include <string>
#include <vector>

#include "lang/token.hpp"

namespace farm::lang {

struct LexError : std::runtime_error {
    int line, col;
    LexError(const std::string& msg, int l, int c)
        : std::runtime_error(msg), line(l), col(c) {}
};

// Tokenize source. Throws LexError on an invalid character / overflow.
// The returned stream always ends with a single Tok::Eof.
std::vector<Token> lex(const std::string& src);

}  // namespace farm::lang
