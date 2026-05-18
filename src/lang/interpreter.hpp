#pragma once
#include "lang/ast.hpp"
#include "lang/value.hpp"

namespace farm::lang {

// Tree-walking interpreter — the M1 reference semantics. The bytecode VM
// (M1.4) must produce identical results for the same program + host.
// Returns the top-level `return` value, or Int(0) if the program falls off
// the end. Throws RuntimeError / LexError / ParseError.
Value interpret(const Program& program, Host& host);
Value interpret(const std::string& source, Host& host);

}  // namespace farm::lang
