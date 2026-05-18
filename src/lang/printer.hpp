#pragma once
#include <string>

#include "lang/ast.hpp"

namespace farm::lang {

// Deterministic canonical pretty-printer (AST -> source). The output
// re-parses to a structurally identical AST, so print() is idempotent and
// usable as the text view in M5's bidirectional sync.
std::string print(const Program& p);
std::string print_expr(const Expr& e);  // exposed for tests

}  // namespace farm::lang
