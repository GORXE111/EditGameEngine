#include "lang/interpreter.hpp"

#include <unordered_map>

#include "lang/parser.hpp"

namespace farm::lang {
namespace {

constexpr int kMaxCallDepth = 4000;  // guard engine stack vs runaway code

[[noreturn]] void rterr(const std::string& m) { throw RuntimeError(m); }

void need(bool ok, const char* m) {
    if (!ok) rterr(m);
}

struct Frame {
    std::unordered_map<std::string, Value> vars;
    Frame* parent_global;  // nullptr if this *is* global
};

struct Interp {
    Host& host;
    std::unordered_map<std::string, const Stmt*> funcs;
    Frame global{{}, nullptr};
    int depth = 0;

    explicit Interp(Host& h) : host(h) {}

    void collect_funcs(const std::vector<StmtPtr>& body) {
        for (auto& s : body) {
            if (s->kind == StmtKind::FuncDecl) {
                if (funcs.count(s->name))
                    rterr("duplicate function '" + s->name + "'");
                funcs[s->name] = s.get();
            }
            collect_funcs(s->body);
            collect_funcs(s->else_body);
        }
    }

    Value* lookup(Frame& f, const std::string& n) {
        if (auto it = f.vars.find(n); it != f.vars.end()) return &it->second;
        if (f.parent_global) {
            auto it = f.parent_global->vars.find(n);
            if (it != f.parent_global->vars.end()) return &it->second;
        }
        return nullptr;
    }

    // ---- expression evaluation ----
    Value eval(const Expr& e, Frame& f) {
        switch (e.kind) {
            case ExprKind::IntLit: return Value::I(e.ival);
            case ExprKind::BoolLit: return Value::B(e.bval);
            case ExprKind::Ident: {
                Value* v = lookup(f, e.name);
                if (!v) rterr("use of undefined variable '" + e.name + "'");
                return *v;
            }
            case ExprKind::Unary: {
                Value r = eval(*e.rhs, f);
                if (e.op == Tok::KwNot) {
                    need(r.is_bool(), "'not' requires a bool");
                    return Value::B(!r.boolean());
                }
                need(r.is_int(), "unary '-' requires an int");
                return Value::I(-r.i);
            }
            case ExprKind::Binary: return eval_binary(e, f);
            case ExprKind::Call: return eval_call(e, f);
        }
        rterr("bad expr");
    }

    Value eval_binary(const Expr& e, Frame& f) {
        // Short-circuit logical operators.
        if (e.op == Tok::KwAnd || e.op == Tok::KwOr) {
            Value l = eval(*e.lhs, f);
            need(l.is_bool(), "logical operator requires bool operands");
            if (e.op == Tok::KwAnd && !l.boolean()) return Value::B(false);
            if (e.op == Tok::KwOr && l.boolean()) return Value::B(true);
            Value r = eval(*e.rhs, f);
            need(r.is_bool(), "logical operator requires bool operands");
            return Value::B(r.boolean());
        }

        Value l = eval(*e.lhs, f);
        Value r = eval(*e.rhs, f);

        switch (e.op) {
            case Tok::Eq:
                need(l.t == r.t, "'==' requires same-type operands");
                return Value::B(l == r);
            case Tok::Ne:
                need(l.t == r.t, "'!=' requires same-type operands");
                return Value::B(!(l == r));
            default: break;
        }

        need(l.is_int() && r.is_int(),
             "arithmetic/comparison requires int operands");
        switch (e.op) {
            case Tok::Plus: return Value::I(l.i + r.i);
            case Tok::Minus: return Value::I(l.i - r.i);
            case Tok::Star: return Value::I(l.i * r.i);
            case Tok::Slash:
                if (r.i == 0) rterr("division by zero");
                return Value::I(l.i / r.i);
            case Tok::Percent:
                if (r.i == 0) rterr("modulo by zero");
                return Value::I(l.i % r.i);
            case Tok::Lt: return Value::B(l.i < r.i);
            case Tok::Le: return Value::B(l.i <= r.i);
            case Tok::Gt: return Value::B(l.i > r.i);
            case Tok::Ge: return Value::B(l.i >= r.i);
            default: rterr("bad binary operator");
        }
    }

    Value eval_call(const Expr& e, Frame& f) {
        std::vector<Value> args;
        args.reserve(e.args.size());
        for (auto& a : e.args) args.push_back(eval(*a, f));

        auto it = funcs.find(e.name);
        if (it == funcs.end()) return host.call(e.name, args);  // native

        const Stmt& fn = *it->second;
        if (args.size() != fn.params.size())
            rterr("function '" + e.name + "' expects " +
                  std::to_string(fn.params.size()) + " args, got " +
                  std::to_string(args.size()));
        if (++depth > kMaxCallDepth) {
            --depth;
            rterr("call depth exceeded (runaway recursion?)");
        }
        Frame local{{}, &global};
        for (size_t i = 0; i < fn.params.size(); ++i)
            local.vars[fn.params[i]] = args[i];
        Value ret = Value::I(0);
        exec_block(fn.body, local, &ret);
        --depth;
        return ret;
    }

    // ---- statement execution; returns true if a `return` fired ----
    bool exec_block(const std::vector<StmtPtr>& body, Frame& f, Value* ret) {
        for (auto& s : body)
            if (exec(*s, f, ret)) return true;
        return false;
    }

    bool exec(const Stmt& s, Frame& f, Value* ret) {
        switch (s.kind) {
            case StmtKind::FuncDecl:
                return false;  // hoisted in collect_funcs
            case StmtKind::Assign: {
                Value v = eval(*s.expr, f);
                if (Value* slot = lookup(f, s.name)) *slot = v;
                else f.vars[s.name] = v;  // declare in current scope
                return false;
            }
            case StmtKind::ExprStmt:
                eval(*s.expr, f);
                return false;
            case StmtKind::Return:
                *ret = s.has_value ? eval(*s.expr, f) : Value::I(0);
                return true;
            case StmtKind::If: {
                Value c = eval(*s.expr, f);
                need(c.is_bool(), "'if' condition must be bool");
                return exec_block(c.boolean() ? s.body : s.else_body, f, ret);
            }
            case StmtKind::While: {
                for (;;) {
                    Value c = eval(*s.expr, f);
                    need(c.is_bool(), "'while' condition must be bool");
                    if (!c.boolean()) break;
                    if (exec_block(s.body, f, ret)) return true;
                }
                return false;
            }
            case StmtKind::Repeat: {
                Value n = eval(*s.expr, f);
                need(n.is_int(), "'repeat' count must be int");
                for (int64_t k = 0; k < n.i; ++k)
                    if (exec_block(s.body, f, ret)) return true;
                return false;
            }
        }
        return false;
    }

    Value run(const Program& p) {
        collect_funcs(p.stmts);
        Value ret = Value::I(0);
        exec_block(p.stmts, global, &ret);
        return ret;
    }
};

}  // namespace

Value interpret(const Program& program, Host& host) {
    return Interp(host).run(program);
}

Value interpret(const std::string& source, Host& host) {
    return interpret(parse(source), host);
}

}  // namespace farm::lang
