#pragma once
#include <stdexcept>
#include <string>

#include "lang/ast.hpp"

namespace farm::lang {

struct ParseError : std::runtime_error {
    int line, col;
    ParseError(const std::string& msg, int l, int c)
        : std::runtime_error(msg), line(l), col(c) {}
};

// Lex + parse source into a Program. Throws LexError or ParseError.
// Node ids are assigned in source order starting at 1.
Program parse(const std::string& src);

}  // namespace farm::lang
