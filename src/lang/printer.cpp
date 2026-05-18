#include "lang/printer.hpp"

#include <sstream>

namespace farm::lang {
namespace {

int prec_of(const Expr& e) {
    switch (e.kind) {
        case ExprKind::Binary:
            switch (e.op) {
                case Tok::KwOr: return 1;
                case Tok::KwAnd: return 2;
                case Tok::Eq: case Tok::Ne: case Tok::Lt:
                case Tok::Le: case Tok::Gt: case Tok::Ge: return 3;
                case Tok::Plus: case Tok::Minus: return 4;
                case Tok::Star: case Tok::Slash: case Tok::Percent: return 5;
                default: return 0;
            }
        case ExprKind::Unary: return 6;
        default: return 100;  // atom: literal / ident / call
    }
}

const char* binop_text(Tok t) {
    switch (t) {
        case Tok::KwOr: return "or";
        case Tok::KwAnd: return "and";
        case Tok::Eq: return "==";
        case Tok::Ne: return "!=";
        case Tok::Lt: return "<";
        case Tok::Le: return "<=";
        case Tok::Gt: return ">";
        case Tok::Ge: return ">=";
        case Tok::Plus: return "+";
        case Tok::Minus: return "-";
        case Tok::Star: return "*";
        case Tok::Slash: return "/";
        case Tok::Percent: return "%";
        default: return "?";
    }
}

struct Printer {
    std::ostringstream os;

    void expr(const Expr& e, int parent_prec, bool right_child) {
        switch (e.kind) {
            case ExprKind::IntLit: os << e.ival; return;
            case ExprKind::BoolLit: os << (e.bval ? "true" : "false"); return;
            case ExprKind::Ident: os << e.name; return;
            case ExprKind::Call: {
                os << e.name << '(';
                for (size_t i = 0; i < e.args.size(); ++i) {
                    if (i) os << ", ";
                    expr(*e.args[i], 1, false);
                }
                os << ')';
                return;
            }
            case ExprKind::Unary: {
                if (e.op == Tok::KwNot) os << "not ";
                else os << '-';
                expr(*e.rhs, 6, false);
                return;
            }
            case ExprKind::Binary: {
                int p = prec_of(e);
                // left-assoc: wrap left when strictly lower, right when <=.
                bool wrap = right_child ? (p <= parent_prec)
                                        : (p < parent_prec);
                if (wrap) os << '(';
                expr(*e.lhs, p, false);
                os << ' ' << binop_text(e.op) << ' ';
                expr(*e.rhs, p, true);
                if (wrap) os << ')';
                return;
            }
        }
    }

    void indent(int n) { for (int i = 0; i < n; ++i) os << "  "; }

    void block(const std::vector<StmtPtr>& b, int depth) {
        for (auto& s : b) stmt(*s, depth);
    }

    void stmt(const Stmt& s, int depth) {
        indent(depth);
        switch (s.kind) {
            case StmtKind::Assign:
                os << s.name << " = ";
                expr(*s.expr, 1, false);
                os << '\n';
                return;
            case StmtKind::ExprStmt:
                expr(*s.expr, 1, false);
                os << '\n';
                return;
            case StmtKind::Return:
                os << "return";
                if (s.has_value) { os << ' '; expr(*s.expr, 1, false); }
                os << '\n';
                return;
            case StmtKind::If:
                os << "if ";
                expr(*s.expr, 1, false);
                os << " then\n";
                block(s.body, depth + 1);
                if (!s.else_body.empty()) {
                    indent(depth);
                    os << "else\n";
                    block(s.else_body, depth + 1);
                }
                indent(depth);
                os << "end\n";
                return;
            case StmtKind::While:
                os << "while ";
                expr(*s.expr, 1, false);
                os << " do\n";
                block(s.body, depth + 1);
                indent(depth);
                os << "end\n";
                return;
            case StmtKind::Repeat:
                os << "repeat ";
                expr(*s.expr, 1, false);
                os << " do\n";
                block(s.body, depth + 1);
                indent(depth);
                os << "end\n";
                return;
            case StmtKind::FuncDecl:
                os << "func " << s.name << '(';
                for (size_t i = 0; i < s.params.size(); ++i) {
                    if (i) os << ", ";
                    os << s.params[i];
                }
                os << ")\n";
                block(s.body, depth + 1);
                indent(depth);
                os << "end\n";
                return;
        }
    }
};

}  // namespace

std::string print(const Program& p) {
    Printer pr;
    for (auto& s : p.stmts) pr.stmt(*s, 0);
    return pr.os.str();
}

std::string print_expr(const Expr& e) {
    Printer pr;
    pr.expr(e, 1, false);
    return pr.os.str();
}

}  // namespace farm::lang
