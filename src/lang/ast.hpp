#pragma once
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "lang/token.hpp"

// FarmCode v1 AST. This is the single source of truth: text and (later)
// blueprint graphs are both projections of it. Every node carries a stable
// NodeId so M5 can key blueprint layout by node identity.
namespace farm::lang {

using NodeId = uint32_t;
constexpr NodeId kNoNode = 0;

enum class ExprKind { IntLit, BoolLit, Ident, Unary, Binary, Call };

struct Expr {
    NodeId id = kNoNode;
    ExprKind kind;
    int line = 0, col = 0;

    int64_t ival = 0;        // IntLit
    bool bval = false;       // BoolLit
    std::string name;        // Ident name / Call callee
    Tok op{};                // Unary / Binary operator

    std::unique_ptr<Expr> lhs;                 // Binary left
    std::unique_ptr<Expr> rhs;                 // Unary operand / Binary right
    std::vector<std::unique_ptr<Expr>> args;   // Call arguments
};

enum class StmtKind { Assign, If, While, Repeat, FuncDecl, Return, ExprStmt };

struct Stmt {
    NodeId id = kNoNode;
    StmtKind kind;
    int line = 0, col = 0;

    std::string name;                  // Assign target / FuncDecl name
    std::vector<std::string> params;   // FuncDecl parameters

    // Assign value / If|While cond / Repeat count / Return value / ExprStmt.
    std::unique_ptr<Expr> expr;
    bool has_value = false;            // Return: whether expr is present

    std::vector<std::unique_ptr<Stmt>> body;       // then / loop / func body
    std::vector<std::unique_ptr<Stmt>> else_body;  // If else branch
};

struct Program {
    std::vector<std::unique_ptr<Stmt>> stmts;
};

using ExprPtr = std::unique_ptr<Expr>;
using StmtPtr = std::unique_ptr<Stmt>;

}  // namespace farm::lang
