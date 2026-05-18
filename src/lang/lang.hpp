#pragma once
#include <string>

// FarmCode language front-end: lexer / parser / AST / bytecode compiler.
// Engine-independent. M1 fills this in.
namespace farm::lang {

// Placeholder so the build graph and headless tests are exercised in M0.
std::string version();

}  // namespace farm::lang
