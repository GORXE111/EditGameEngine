#pragma once
#include <stdexcept>
#include <string>

#include "lang/ast.hpp"
#include "lang/feature.hpp"

namespace farm::lang {

struct ParseError : std::runtime_error {
    int line, col;
    ParseError(const std::string& msg, int l, int c)
        : std::runtime_error(msg), line(l), col(c) {}
};

// Lex + parse source into a Program. Throws LexError or ParseError.
// Node ids are assigned in source order starting at 1.
Program parse(const std::string& src);

// Parse, then reject any control-flow construct not present in `fs`
// (progression gating). Throws ParseError pointing at the locked node.
Program parse(const std::string& src, const FeatureSet& fs);
void require_features(const Program& p, const FeatureSet& fs);

}  // namespace farm::lang
