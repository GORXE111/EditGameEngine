#pragma once
#include "blueprint/graph.hpp"
#include "lang/ast.hpp"

// AST <-> blueprint graph codec. build() is total; to_ast() reconstructs an
// equivalent AST (same structure, same NodeIds). Round-trip is verified
// against the canonical printer in the headless tests.
namespace farm::blueprint {

Graph build(const farm::lang::Program& program);
farm::lang::Program to_ast(const Graph& graph);

}  // namespace farm::blueprint
