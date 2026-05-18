#pragma once
#include <unordered_map>
#include <utility>

#include "blueprint/graph.hpp"
#include "lang/ast.hpp"

// AST <-> blueprint graph codec. build() is total; to_ast() reconstructs an
// equivalent AST (same structure, same NodeIds). Round-trip is verified
// against the canonical printer in the headless tests.
namespace farm::blueprint {

// Node positions keyed by AST NodeId, so the blueprint layout survives when
// the graph is re-derived after an edit (M5 isomorphic sync).
using LayoutStore =
    std::unordered_map<farm::lang::NodeId, std::pair<float, float>>;

Graph build(const farm::lang::Program& program);
// Same as build(), but seeds positions of nodes whose AST id is in `prev`
// from that store; nodes not in `prev` keep the auto-layout.
Graph build(const farm::lang::Program& program, const LayoutStore& prev);

LayoutStore capture_layout(const Graph& graph);

farm::lang::Program to_ast(const Graph& graph);

}  // namespace farm::blueprint
