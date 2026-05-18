#include "blueprint/codec.hpp"

namespace farm::blueprint {
using namespace farm::lang;

namespace {

constexpr float kCol = 260.f;   // exec indent per nesting level
constexpr float kRow = 90.f;    // vertical spacing on a chain
constexpr float kERow = 70.f;   // vertical spacing for expr nodes

struct Builder {
    Graph g;
    int row = 0;     // running row for statement nodes
    int erow = 0;    // running row for expression nodes

    int expr(const Expr& e) {
        int idx = -1;
        switch (e.kind) {
            case ExprKind::IntLit:
                idx = g.add(NK::IntLit).id;
                g.nodes[idx].ival = e.ival;
                break;
            case ExprKind::BoolLit:
                idx = g.add(NK::BoolLit).id;
                g.nodes[idx].bval = e.bval;
                break;
            case ExprKind::Ident:
                idx = g.add(NK::VarGet).id;
                g.nodes[idx].name = e.name;
                break;
            case ExprKind::Unary: {
                idx = g.add(NK::Unary).id;
                g.nodes[idx].op = e.op;
                int r = expr(*e.rhs);
                g.nodes[idx].in = {r};
                break;
            }
            case ExprKind::Binary: {
                idx = g.add(NK::Binary).id;
                g.nodes[idx].op = e.op;
                int l = expr(*e.lhs);
                int r = expr(*e.rhs);
                g.nodes[idx].in = {l, r};
                break;
            }
            case ExprKind::Call: {
                idx = g.add(NK::CallExpr).id;
                g.nodes[idx].name = e.name;
                std::vector<int> args;
                for (auto& a : e.args) args.push_back(expr(*a));
                g.nodes[idx].in = std::move(args);
                break;
            }
        }
        Node& n = g.nodes[idx];
        n.ast_id = e.id;
        n.x = -250.f;
        n.y = static_cast<float>(erow++) * kERow;
        return idx;
    }

    // Returns head node index of the chain (-1 if empty).
    int chain(const std::vector<StmtPtr>& body, int col) {
        int head = -1, prev = -1;
        for (auto& s : body) {
            int n = stmt(*s, col);
            if (head == -1) head = n;
            if (prev != -1) g.nodes[prev].exec_next = n;
            prev = n;
        }
        return head;
    }

    int stmt(const Stmt& s, int col) {
        int idx = -1;
        switch (s.kind) {
            case StmtKind::Assign:
                idx = g.add(NK::Assign).id;
                g.nodes[idx].name = s.name;
                g.nodes[idx].in = {expr(*s.expr)};
                break;
            case StmtKind::ExprStmt:  // s.expr is a Call
                idx = g.add(NK::CallStmt).id;
                g.nodes[idx].name = s.expr->name;
                {
                    std::vector<int> args;
                    for (auto& a : s.expr->args) args.push_back(expr(*a));
                    g.nodes[idx].in = std::move(args);
                }
                break;
            case StmtKind::If: {
                idx = g.add(NK::If).id;
                g.nodes[idx].in = {expr(*s.expr)};
                int t = chain(s.body, col + 1);
                int e = chain(s.else_body, col + 1);
                g.nodes[idx].body = t;
                g.nodes[idx].els = e;
                break;
            }
            case StmtKind::While: {
                idx = g.add(NK::While).id;
                g.nodes[idx].in = {expr(*s.expr)};
                g.nodes[idx].body = chain(s.body, col + 1);
                break;
            }
            case StmtKind::Repeat: {
                idx = g.add(NK::Repeat).id;
                g.nodes[idx].in = {expr(*s.expr)};
                g.nodes[idx].body = chain(s.body, col + 1);
                break;
            }
            case StmtKind::Return:
                idx = g.add(NK::Return).id;
                g.nodes[idx].has_value = s.has_value;
                if (s.has_value) g.nodes[idx].in = {expr(*s.expr)};
                break;
            case StmtKind::FuncDecl: {
                idx = g.add(NK::FuncDef).id;
                g.nodes[idx].name = s.name;
                g.nodes[idx].params = s.params;
                g.nodes[idx].body = chain(s.body, col + 1);
                break;
            }
        }
        Node& n = g.nodes[idx];
        n.ast_id = s.id;
        n.x = static_cast<float>(col) * kCol;
        n.y = static_cast<float>(row++) * kRow;
        return idx;
    }
};

struct Rebuilder {
    const Graph& g;

    ExprPtr expr(int idx) {
        const Node& n = g.nodes[idx];
        auto e = std::make_unique<Expr>();
        e->id = n.ast_id;
        switch (n.kind) {
            case NK::IntLit:
                e->kind = ExprKind::IntLit;
                e->ival = n.ival;
                break;
            case NK::BoolLit:
                e->kind = ExprKind::BoolLit;
                e->bval = n.bval;
                break;
            case NK::VarGet:
                e->kind = ExprKind::Ident;
                e->name = n.name;
                break;
            case NK::Unary:
                e->kind = ExprKind::Unary;
                e->op = n.op;
                e->rhs = expr(n.in[0]);
                break;
            case NK::Binary:
                e->kind = ExprKind::Binary;
                e->op = n.op;
                e->lhs = expr(n.in[0]);
                e->rhs = expr(n.in[1]);
                break;
            case NK::CallExpr:
                e->kind = ExprKind::Call;
                e->name = n.name;
                for (int a : n.in) e->args.push_back(expr(a));
                break;
            default:
                e->kind = ExprKind::IntLit;  // unreachable for valid graphs
                break;
        }
        return e;
    }

    std::vector<StmtPtr> chain(int idx) {
        std::vector<StmtPtr> out;
        while (idx != -1) {
            out.push_back(stmt(idx));
            idx = g.nodes[idx].exec_next;
        }
        return out;
    }

    StmtPtr stmt(int idx) {
        const Node& n = g.nodes[idx];
        auto s = std::make_unique<Stmt>();
        s->id = n.ast_id;
        switch (n.kind) {
            case NK::Assign:
                s->kind = StmtKind::Assign;
                s->name = n.name;
                s->expr = expr(n.in[0]);
                break;
            case NK::CallStmt: {
                s->kind = StmtKind::ExprStmt;
                auto call = std::make_unique<Expr>();
                call->id = n.ast_id;
                call->kind = ExprKind::Call;
                call->name = n.name;
                for (int a : n.in) call->args.push_back(expr(a));
                s->expr = std::move(call);
                break;
            }
            case NK::If:
                s->kind = StmtKind::If;
                s->expr = expr(n.in[0]);
                s->body = chain(n.body);
                s->else_body = chain(n.els);
                break;
            case NK::While:
                s->kind = StmtKind::While;
                s->expr = expr(n.in[0]);
                s->body = chain(n.body);
                break;
            case NK::Repeat:
                s->kind = StmtKind::Repeat;
                s->expr = expr(n.in[0]);
                s->body = chain(n.body);
                break;
            case NK::Return:
                s->kind = StmtKind::Return;
                s->has_value = n.has_value;
                if (n.has_value) s->expr = expr(n.in[0]);
                break;
            case NK::FuncDef:
                s->kind = StmtKind::FuncDecl;
                s->name = n.name;
                s->params = n.params;
                s->body = chain(n.body);
                break;
            default:
                s->kind = StmtKind::Return;  // unreachable for valid graphs
                break;
        }
        return s;
    }
};

}  // namespace

Graph build(const Program& program) {
    Builder b;
    int entry = b.g.add(NK::Entry).id;
    b.g.entry = entry;
    int head = b.chain(program.stmts, 0);
    b.g.nodes[entry].exec_next = head;
    b.g.nodes[entry].x = -520.f;
    b.g.nodes[entry].y = 0.f;
    return std::move(b.g);
}

Graph build(const Program& program, const LayoutStore& prev) {
    Graph g = build(program);
    for (auto& n : g.nodes) {
        if (n.ast_id == farm::lang::kNoNode) continue;
        auto it = prev.find(n.ast_id);
        if (it != prev.end()) {
            n.x = it->second.first;
            n.y = it->second.second;
        }
    }
    return g;
}

LayoutStore capture_layout(const Graph& g) {
    LayoutStore ls;
    for (const auto& n : g.nodes)
        if (n.ast_id != farm::lang::kNoNode)
            ls[n.ast_id] = {n.x, n.y};
    return ls;
}

Program to_ast(const Graph& g) {
    Program p;
    if (g.entry < 0) return p;
    Rebuilder r{g};
    p.stmts = r.chain(g.nodes[g.entry].exec_next);
    return p;
}

}  // namespace farm::blueprint
