#pragma once
#include "lang/ast.hpp"
#include "vm/bytecode.hpp"

namespace farm::vm {

// Compile a parsed Program into a Chunk. User functions (anywhere in the
// tree) are hoisted into a flat function namespace, matching the tree
// interpreter. Throws farm::lang::RuntimeError on duplicate functions.
Chunk compile(const farm::lang::Program& program);

}  // namespace farm::vm
