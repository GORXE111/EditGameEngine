#include "lang/feature.hpp"

#include "lang/parser.hpp"

namespace farm::lang {

const char* feature_name(Feature f) {
    switch (f) {
        case Feature::Conditionals: return "Conditionals (if/else)";
        case Feature::Loops: return "Loops (while)";
        case Feature::RepeatLoop: return "Repeat";
        case Feature::Functions: return "Functions (func)";
        case Feature::Lists: return "Lists ([], indexing, len/append)";
    }
    return "?";
}

namespace {

void check_block(const std::vector<StmtPtr>& body, const FeatureSet& fs);

[[noreturn]] void locked(Feature f, int line, int col) {
    throw ParseError(std::string("locked feature: ") + feature_name(f) +
                         " — unlock it first",
                     line, col);
}

void check_expr(const Expr& e, const FeatureSet& fs) {
    if ((e.kind == ExprKind::ListLit || e.kind == ExprKind::Index) &&
        !fs.has(Feature::Lists))
        locked(Feature::Lists, e.line, e.col);
    if (e.kind == ExprKind::Call &&
        (e.name == "len" || e.name == "append") &&
        !fs.has(Feature::Lists))
        locked(Feature::Lists, e.line, e.col);
    if (e.lhs) check_expr(*e.lhs, fs);
    if (e.rhs) check_expr(*e.rhs, fs);
    for (auto& a : e.args) check_expr(*a, fs);
}

void check_stmt(const Stmt& s, const FeatureSet& fs) {
    switch (s.kind) {
        case StmtKind::If:
            if (!fs.has(Feature::Conditionals))
                locked(Feature::Conditionals, s.line, s.col);
            break;
        case StmtKind::While:
            if (!fs.has(Feature::Loops)) locked(Feature::Loops, s.line, s.col);
            break;
        case StmtKind::Repeat:
            if (!fs.has(Feature::RepeatLoop))
                locked(Feature::RepeatLoop, s.line, s.col);
            break;
        case StmtKind::FuncDecl:
            if (!fs.has(Feature::Functions))
                locked(Feature::Functions, s.line, s.col);
            break;
        case StmtKind::SetIndex:
            if (!fs.has(Feature::Lists)) locked(Feature::Lists, s.line, s.col);
            break;
        default:
            break;
    }
    if (s.expr) check_expr(*s.expr, fs);
    if (s.target) check_expr(*s.target, fs);
    check_block(s.body, fs);
    check_block(s.else_body, fs);
}

void check_block(const std::vector<StmtPtr>& body, const FeatureSet& fs) {
    for (auto& s : body) check_stmt(*s, fs);
}

}  // namespace

void require_features(const Program& p, const FeatureSet& fs) {
    check_block(p.stmts, fs);
}

Program parse(const std::string& src, const FeatureSet& fs) {
    Program p = parse(src);
    require_features(p, fs);
    return p;
}

}  // namespace farm::lang
