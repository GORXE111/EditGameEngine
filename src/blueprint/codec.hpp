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

// Throws farm::lang::RuntimeError if the graph references unwired pins
// (e.g. an If whose condition source is -1, or an exec edge into a deleted
// node). Otherwise returns the rebuilt Program.
farm::lang::Program to_ast(const Graph& graph);

// ---- structural-editing helpers (E4) ----

// How many value inputs a freshly-spawned node of this kind/native should
// start with. Robot natives have known arities; generic Call starts at 0
// and the player can add more.
int default_arity(NK kind);
int native_arity(const std::string& callee_name);

// Append a new node with the given kind, default arity & sentinel in[] = -1.
// Returns the new node index. Position not set; caller assigns x/y.
int spawn(Graph& g, NK kind);

// Drop tombstoned nodes from `g`, remap all edges to new indices. Safe to
// call multiple times; idempotent for already-compact graphs.
void compact(Graph& g);

}  // namespace farm::blueprint
