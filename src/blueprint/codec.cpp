#include "blueprint/codec.hpp"

#include "lang/value.hpp"  // RuntimeError

namespace farm::blueprint {
using namespace farm::lang;

int default_arity(NK k) {
    switch (k) {
        case NK::Assign:    return 1;
        case NK::If:        return 1;
        case NK::While:     return 1;
        case NK::Repeat:    return 1;
        case NK::Return:    return 1;  // optional; -1 leaves it value-less
        case NK::Unary:     return 1;
        case NK::Binary:    return 2;
        case NK::IndexGet:  return 2;
        case NK::SetIndex:  return 3;
        default:            return 0;  // Entry/Call*/Lit/VarGet/ListLit
    }
}

int native_arity(const std::string& n) {
    if (n == "move" || n == "plant" || n == "inventory" ||
        n == "num_items" || n == "unlock" || n == "get_cost" ||
        n == "num_unlocked" || n == "len")
        return 1;
    if (n == "append") return 2;
    return 0;  // till / water / fertilize / harvest / wait / sense / pos /
               // can_harvest / get_drone_id / num_drones / unknown
}

int spawn(Graph& g, NK k) {
    Node& n = g.add(k);
    n.in.assign(default_arity(k), -1);
    return n.id;
}

void compact(Graph& g) {
    const int N = static_cast<int>(g.nodes.size());
    std::vector<int> remap(N, -1);
    int next = 0;
    for (int i = 0; i < N; ++i)
        if (!g.nodes[i].deleted) remap[i] = next++;

    auto remap_one = [&](int& idx) {
        idx = (idx >= 0 && idx < N) ? remap[idx] : -1;
    };

    std::vector<Node> out;
    out.reserve(next);
    for (int i = 0; i < N; ++i) {
        if (g.nodes[i].deleted) continue;
        Node m = std::move(g.nodes[i]);
        remap_one(m.exec_next);
        remap_one(m.body);
        remap_one(m.els);
        for (auto& v : m.in) remap_one(v);
        m.id = static_cast<int>(out.size());
        out.push_back(std::move(m));
    }
    if (g.entry >= 0 && g.entry < N) g.entry = remap[g.entry];
    g.nodes = std::move(out);
}

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
            case ExprKind::ListLit: {
                idx = g.add(NK::ListLit).id;
                std::vector<int> els;
                for (auto& a : e.args) els.push_back(expr(*a));
                g.nodes[idx].in = std::move(els);
                break;
            }
            case ExprKind::Index: {
                idx = g.add(NK::IndexGet).id;
                int c = expr(*e.lhs);
                int i = expr(*e.rhs);
                g.nodes[idx].in = {c, i};
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
            case StmtKind::SetIndex: {
                idx = g.add(NK::SetIndex).id;
                int c = expr(*s.target->lhs);
                int i = expr(*s.target->rhs);
                int v = expr(*s.expr);
                g.nodes[idx].in = {c, i, v};
                break;
            }
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

// Tiny helper: a friendly node description for error messages.
static std::string node_desc(const Node& n) {
    switch (n.kind) {
        case NK::Entry:    return "Entry";
        case NK::Assign:   return "Set '" + n.name + "'";
        case NK::CallStmt: case NK::CallExpr:
            return n.name + "()";
        case NK::If:       return "If";
        case NK::While:    return "While";
        case NK::Repeat:   return "Repeat";
        case NK::Return:   return "Return";
        case NK::FuncDef:  return "func " + n.name;
        case NK::SetIndex: return "Set Element";
        case NK::IntLit:   return std::to_string(n.ival);
        case NK::BoolLit:  return n.bval ? "true" : "false";
        case NK::VarGet:   return n.name;
        case NK::Unary:    return "(unary)";
        case NK::Binary:   return "(binary)";
        case NK::ListLit:  return "[list]";
        case NK::IndexGet: return "(index)";
    }
    return "(node)";
}

struct Rebuilder {
    const Graph& g;

    // Validate an index into g.nodes. Throws RuntimeError with a player-
    // facing message naming the host node and the missing role.
    const Node& deref(int idx, const Node& host, const char* role) {
        if (idx < 0) {
            throw RuntimeError("blueprint: '" + node_desc(host) +
                               "' is missing a wire on '" + role + "'");
        }
        if (idx >= static_cast<int>(g.nodes.size()) ||
            g.nodes[idx].deleted) {
            throw RuntimeError("blueprint: '" + node_desc(host) +
                               "' wire on '" + role + "' targets a "
                               "deleted node");
        }
        return g.nodes[idx];
    }

    // expr-in-host: validate idx wrt host node + role, then build expr().
    ExprPtr expr_in(const Node& host, int idx, const char* role) {
        deref(idx, host, role);
        return expr(idx);
    }

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
                e->rhs = expr_in(n, n.in.empty() ? -1 : n.in[0], "x");
                break;
            case NK::Binary:
                e->kind = ExprKind::Binary;
                e->op = n.op;
                e->lhs = expr_in(n, n.in.size() > 0 ? n.in[0] : -1, "a");
                e->rhs = expr_in(n, n.in.size() > 1 ? n.in[1] : -1, "b");
                break;
            case NK::CallExpr:
                e->kind = ExprKind::Call;
                e->name = n.name;
                for (size_t k = 0; k < n.in.size(); ++k)
                    e->args.push_back(expr_in(n, n.in[k], "arg"));
                break;
            case NK::ListLit:
                e->kind = ExprKind::ListLit;
                for (size_t k = 0; k < n.in.size(); ++k)
                    e->args.push_back(expr_in(n, n.in[k], "element"));
                break;
            case NK::IndexGet:
                e->kind = ExprKind::Index;
                e->lhs = expr_in(n, n.in.size() > 0 ? n.in[0] : -1, "list");
                e->rhs = expr_in(n, n.in.size() > 1 ? n.in[1] : -1, "index");
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
            if (idx < 0 || idx >= static_cast<int>(g.nodes.size()) ||
                g.nodes[idx].deleted)
                throw RuntimeError(
                    "blueprint: exec chain reaches a deleted node");
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
                s->expr = expr_in(n, n.in.empty() ? -1 : n.in[0], "value");
                break;
            case NK::SetIndex: {
                s->kind = StmtKind::SetIndex;
                auto tgt = std::make_unique<Expr>();
                tgt->id = n.ast_id;
                tgt->kind = ExprKind::Index;
                tgt->lhs = expr_in(n, n.in.size() > 0 ? n.in[0] : -1,
                                   "list");
                tgt->rhs = expr_in(n, n.in.size() > 1 ? n.in[1] : -1,
                                   "index");
                s->target = std::move(tgt);
                s->expr = expr_in(n, n.in.size() > 2 ? n.in[2] : -1,
                                  "value");
                break;
            }
            case NK::CallStmt: {
                s->kind = StmtKind::ExprStmt;
                auto call = std::make_unique<Expr>();
                call->id = n.ast_id;
                call->kind = ExprKind::Call;
                call->name = n.name;
                for (size_t k = 0; k < n.in.size(); ++k)
                    call->args.push_back(expr_in(n, n.in[k], "arg"));
                s->expr = std::move(call);
                break;
            }
            case NK::If:
                s->kind = StmtKind::If;
                s->expr = expr_in(n, n.in.empty() ? -1 : n.in[0], "cond");
                s->body = chain(n.body);
                s->else_body = chain(n.els);
                break;
            case NK::While:
                s->kind = StmtKind::While;
                s->expr = expr_in(n, n.in.empty() ? -1 : n.in[0], "cond");
                s->body = chain(n.body);
                break;
            case NK::Repeat:
                s->kind = StmtKind::Repeat;
                s->expr = expr_in(n, n.in.empty() ? -1 : n.in[0], "count");
                s->body = chain(n.body);
                break;
            case NK::Return:
                s->kind = StmtKind::Return;
                // Treat "value pin wired" as has-value, regardless of the
                // historical flag (lets the player toggle by wiring/unwiring).
                if (!n.in.empty() && n.in[0] != -1) {
                    s->has_value = true;
                    s->expr = expr_in(n, n.in[0], "value");
                } else {
                    s->has_value = false;
                }
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
