#pragma once
#include <cstdint>
#include <string>
#include <vector>

#include "lang/ast.hpp"
#include "lang/token.hpp"

// Blueprint graph: the node-and-wire projection of the AST. It is the same
// program as the text view (shared AST is the single source of truth). Each
// node carries the AST NodeId so M5 can key layout/sync by node identity.
//
// Edges are role-tagged node indices (faithful + trivially round-trippable),
// rendered as Unreal-style pins/wires by the imnodes view (M4.2):
//   - exec flow: exec_next (sequence), body (then/loop), els (else)
//   - data flow: in[] (operands / args / condition / assigned value)
namespace farm::blueprint {

enum class NK {
    Entry,                                   // program start
    Assign, CallStmt, If, While, Repeat, Return, FuncDef,  // exec/statements
    IntLit, BoolLit, VarGet, Unary, Binary, CallExpr        // value/exprs
};

struct Node {
    farm::lang::NodeId ast_id = farm::lang::kNoNode;
    int id = -1;       // graph-local id == index in Graph::nodes
    NK kind;

    std::string name;                   // Assign target / VarGet / call name
                                        // / FuncDef name
    farm::lang::Tok op{};               // Unary / Binary operator
    int64_t ival = 0;                   // IntLit
    bool bval = false;                  // BoolLit
    std::vector<std::string> params;    // FuncDef
    bool has_value = false;             // Return: has a value input

    float x = 0, y = 0;                 // layout (persisted in M5)

    int exec_next = -1;                 // next statement in this chain
    int body = -1;                      // If 'then' / While|Repeat body head
    int els = -1;                       // If 'else' head
    std::vector<int> in;                // value inputs (see header comment)
};

struct Graph {
    std::vector<Node> nodes;
    int entry = -1;  // index of the Entry node

    Node& add(NK k) {
        Node n;
        n.kind = k;
        n.id = static_cast<int>(nodes.size());
        nodes.push_back(n);
        return nodes.back();
    }
};

}  // namespace farm::blueprint
