#include "vm/compiler.hpp"

#include "lang/value.hpp"  // farm::lang::RuntimeError

namespace farm::vm {
using namespace farm::lang;

namespace {

struct Compiler {
    Chunk chunk;
    int repeat_seq = 0;

    int int_const(int64_t v) {
        for (size_t i = 0; i < chunk.int_consts.size(); ++i)
            if (chunk.int_consts[i] == v) return static_cast<int>(i);
        chunk.int_consts.push_back(v);
        return static_cast<int>(chunk.int_consts.size() - 1);
    }
    int name_idx(const std::string& s) {
        for (size_t i = 0; i < chunk.names.size(); ++i)
            if (chunk.names[i] == s) return static_cast<int>(i);
        chunk.names.push_back(s);
        return static_cast<int>(chunk.names.size() - 1);
    }

    // ---- function-local emission helpers ----
    Function* fn = nullptr;
    int emit(Op op, int32_t a = 0, int32_t b = 0) {
        fn->code.push_back({op, a, b});
        return static_cast<int>(fn->code.size() - 1);
    }
    int here() const { return static_cast<int>(fn->code.size()); }
    void patch(int at) { fn->code[at].a = here(); }

    // ---- discover & register all function declarations ----
    void collect(const std::vector<StmtPtr>& body) {
        for (auto& s : body) {
            if (s->kind == StmtKind::FuncDecl) {
                if (chunk.func_index.count(s->name))
                    throw RuntimeError("duplicate function '" + s->name + "'");
                int idx = static_cast<int>(chunk.functions.size());
                chunk.functions.push_back(Function{s->name, s->params, {}});
                chunk.func_index[s->name] = idx;
            }
            collect(s->body);
            collect(s->else_body);
        }
    }

    // ---- expressions ----
    void expr(const Expr& e) {
        switch (e.kind) {
            case ExprKind::IntLit:
                emit(Op::PushInt, int_const(e.ival));
                return;
            case ExprKind::BoolLit:
                emit(Op::PushBool, e.bval ? 1 : 0);
                return;
            case ExprKind::Ident:
                emit(Op::Load, name_idx(e.name));
                return;
            case ExprKind::Unary:
                expr(*e.rhs);
                emit(e.op == Tok::KwNot ? Op::Not : Op::Neg);
                return;
            case ExprKind::Call:
                for (auto& a : e.args) expr(*a);
                emit(Op::Call, name_idx(e.name),
                     static_cast<int32_t>(e.args.size()));
                return;
            case ExprKind::Binary:
                binary(e);
                return;
            case ExprKind::ListLit:
                for (auto& a : e.args) expr(*a);
                emit(Op::MakeList, static_cast<int32_t>(e.args.size()));
                return;
            case ExprKind::Index:
                expr(*e.lhs);
                expr(*e.rhs);
                emit(Op::IndexGet);
                return;
        }
    }

    void binary(const Expr& e) {
        if (e.op == Tok::KwAnd || e.op == Tok::KwOr) {
            bool is_and = (e.op == Tok::KwAnd);
            Op br = is_and ? Op::JumpFalse : Op::JumpTrue;
            expr(*e.lhs);
            int j1 = emit(br);
            expr(*e.rhs);
            int j2 = emit(br);
            emit(Op::PushBool, is_and ? 1 : 0);
            int jend = emit(Op::Jump);
            patch(j1);
            patch(j2);
            emit(Op::PushBool, is_and ? 0 : 1);
            patch(jend);
            return;
        }
        expr(*e.lhs);
        expr(*e.rhs);
        switch (e.op) {
            case Tok::Plus: emit(Op::Add); break;
            case Tok::Minus: emit(Op::Sub); break;
            case Tok::Star: emit(Op::Mul); break;
            case Tok::Slash: emit(Op::Div); break;
            case Tok::Percent: emit(Op::Mod); break;
            case Tok::Eq: emit(Op::Eq); break;
            case Tok::Ne: emit(Op::Ne); break;
            case Tok::Lt: emit(Op::Lt); break;
            case Tok::Le: emit(Op::Le); break;
            case Tok::Gt: emit(Op::Gt); break;
            case Tok::Ge: emit(Op::Ge); break;
            default: throw RuntimeError("bad binary op in compiler");
        }
    }

    // ---- statements ----
    void block(const std::vector<StmtPtr>& body) {
        for (auto& s : body) stmt(*s);
    }

    void stmt(const Stmt& s) {
        switch (s.kind) {
            case StmtKind::FuncDecl:
                return;  // hoisted
            case StmtKind::Assign:
                expr(*s.expr);
                emit(Op::Store, name_idx(s.name));
                return;
            case StmtKind::ExprStmt:
                expr(*s.expr);
                emit(Op::Pop);
                return;
            case StmtKind::SetIndex:
                expr(*s.target->lhs);  // collection
                expr(*s.target->rhs);  // index
                expr(*s.expr);         // value
                emit(Op::IndexSet);
                return;
            case StmtKind::Return:
                if (s.has_value) { expr(*s.expr); emit(Op::Ret, 0, 1); }
                else emit(Op::Ret, 0, 0);
                return;
            case StmtKind::If: {
                expr(*s.expr);
                int jelse = emit(Op::JumpFalse);
                block(s.body);
                int jend = emit(Op::Jump);
                patch(jelse);
                block(s.else_body);
                patch(jend);
                return;
            }
            case StmtKind::While: {
                int top = here();
                expr(*s.expr);
                int jend = emit(Op::JumpFalse);
                block(s.body);
                emit(Op::Jump, top);
                patch(jend);
                return;
            }
            case StmtKind::Repeat: {
                // desugar: #rep = N; while #rep > 0 do BODY; #rep = #rep-1 end
                std::string ctr = "#rep" + std::to_string(repeat_seq++);
                int cn = name_idx(ctr);
                expr(*s.expr);
                emit(Op::Store, cn);
                int top = here();
                emit(Op::Load, cn);
                emit(Op::PushInt, int_const(0));
                emit(Op::Gt);
                int jend = emit(Op::JumpFalse);
                block(s.body);
                emit(Op::Load, cn);
                emit(Op::PushInt, int_const(1));
                emit(Op::Sub);
                emit(Op::Store, cn);
                emit(Op::Jump, top);
                patch(jend);
                return;
            }
        }
    }

    Chunk run(const Program& p) {
        // functions[0] = synthetic top-level "@main"
        chunk.functions.push_back(Function{"@main", {}, {}});
        chunk.func_index["@main"] = 0;
        collect(p.stmts);

        // compile user-function bodies (resolved by name into entries that
        // `collect` already created)
        compile_func_bodies(p.stmts);

        // top-level
        fn = &chunk.functions[0];
        for (auto& s : p.stmts)
            if (s->kind != StmtKind::FuncDecl) stmt(*s);
        emit(Op::Halt);
        return std::move(chunk);
    }

    void compile_func_bodies(const std::vector<StmtPtr>& body) {
        for (auto& s : body) {
            if (s->kind == StmtKind::FuncDecl) {
                fn = &chunk.functions[chunk.func_index[s->name]];
                for (auto& b : s->body) stmt(*b);
                emit(Op::Ret, 0, 0);  // implicit `return 0` fallthrough
            }
            compile_func_bodies(s->body);
            compile_func_bodies(s->else_body);
        }
    }
};

}  // namespace

Chunk compile(const Program& program) {
    Compiler c;
    return c.run(program);
}

}  // namespace farm::vm
