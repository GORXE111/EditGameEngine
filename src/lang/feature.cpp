#include "lang/feature.hpp"

#include "lang/parser.hpp"

namespace farm::lang {

const char* feature_name(Feature f) {
    switch (f) {
        case Feature::Conditionals: return "Conditionals (if/else)";
        case Feature::Loops: return "Loops (while)";
        case Feature::RepeatLoop: return "Repeat";
        case Feature::Functions: return "Functions (func)";
    }
    return "?";
}

namespace {

void check_block(const std::vector<StmtPtr>& body, const FeatureSet& fs);

void check_stmt(const Stmt& s, const FeatureSet& fs) {
    auto deny = [&](Feature f) {
        if (!fs.has(f))
            throw ParseError(std::string("locked feature: ") +
                                 feature_name(f) + " — unlock it first",
                             s.line, s.col);
    };
    switch (s.kind) {
        case StmtKind::If: deny(Feature::Conditionals); break;
        case StmtKind::While: deny(Feature::Loops); break;
        case StmtKind::Repeat: deny(Feature::RepeatLoop); break;
        case StmtKind::FuncDecl: deny(Feature::Functions); break;
        default: break;  // Assign / ExprStmt / Return are always allowed
    }
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
