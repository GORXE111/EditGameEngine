#include "lang/parser.hpp"

#include "lang/lexer.hpp"

namespace farm::lang {
namespace {

int binary_prec(Tok t) {
    switch (t) {
        case Tok::KwOr: return 1;
        case Tok::KwAnd: return 2;
        case Tok::Eq:
        case Tok::Ne:
        case Tok::Lt:
        case Tok::Le:
        case Tok::Gt:
        case Tok::Ge: return 3;
        case Tok::Plus:
        case Tok::Minus: return 4;
        case Tok::Star:
        case Tok::Slash:
        case Tok::Percent: return 5;
        default: return -1;
    }
}

struct Parser {
    std::vector<Token> toks;
    size_t pos = 0;
    NodeId next_id = 1;

    const Token& peek(size_t off = 0) const {
        size_t i = pos + off;
        return i < toks.size() ? toks[i] : toks.back();  // back() == Eof
    }
    bool at(Tok k) const { return peek().kind == k; }
    const Token& advance() { return toks[pos++]; }

    [[noreturn]] void err(const std::string& m) const {
        throw ParseError(m, peek().line, peek().col);
    }
    const Token& expect(Tok k, const char* what) {
        if (!at(k))
            err(std::string("expected ") + what + ", got '" +
                tok_name(peek().kind) + "'");
        return advance();
    }

    ExprPtr mkexpr(ExprKind k) {
        auto e = std::make_unique<Expr>();
        e->id = next_id++;
        e->kind = k;
        e->line = peek().line;
        e->col = peek().col;
        return e;
    }
    StmtPtr mkstmt(StmtKind k) {
        auto s = std::make_unique<Stmt>();
        s->id = next_id++;
        s->kind = k;
        s->line = peek().line;
        s->col = peek().col;
        return s;
    }

    // ---- expressions (precedence climbing) ----
    ExprPtr parse_expr(int min_prec = 1) {
        ExprPtr lhs = parse_unary();
        for (;;) {
            int prec = binary_prec(peek().kind);
            if (prec < min_prec) break;
            Tok op = advance().kind;
            ExprPtr rhs = parse_expr(prec + 1);  // left-associative
            auto bin = std::make_unique<Expr>();
            bin->id = next_id++;
            bin->kind = ExprKind::Binary;
            bin->op = op;
            bin->line = lhs->line;
            bin->col = lhs->col;
            bin->lhs = std::move(lhs);
            bin->rhs = std::move(rhs);
            lhs = std::move(bin);
        }
        return lhs;
    }

    ExprPtr parse_unary() {
        if (at(Tok::KwNot) || at(Tok::Minus)) {
            auto u = mkexpr(ExprKind::Unary);
            u->op = advance().kind;
            u->rhs = parse_unary();
            return u;
        }
        return parse_primary();
    }

    ExprPtr parse_primary() {
        const Token& t = peek();
        if (t.kind == Tok::Int) {
            auto e = mkexpr(ExprKind::IntLit);
            e->ival = advance().ivalue;
            return e;
        }
        if (t.kind == Tok::KwTrue || t.kind == Tok::KwFalse) {
            auto e = mkexpr(ExprKind::BoolLit);
            e->bval = (advance().kind == Tok::KwTrue);
            return e;
        }
        if (t.kind == Tok::LParen) {
            advance();
            ExprPtr e = parse_expr();
            expect(Tok::RParen, "')'");
            return e;
        }
        if (t.kind == Tok::Ident) {
            std::string name = t.text;
            if (peek(1).kind == Tok::LParen) {
                auto call = mkexpr(ExprKind::Call);
                call->name = name;
                advance();  // ident
                advance();  // (
                if (!at(Tok::RParen)) {
                    call->args.push_back(parse_expr());
                    while (at(Tok::Comma)) {
                        advance();
                        call->args.push_back(parse_expr());
                    }
                }
                expect(Tok::RParen, "')'");
                return call;
            }
            auto e = mkexpr(ExprKind::Ident);
            e->name = name;
            advance();
            return e;
        }
        err(std::string("expected expression, got '") +
            tok_name(t.kind) + "'");
    }

    // ---- statements ----
    bool block_terminator() const {
        return at(Tok::KwEnd) || at(Tok::KwElse) || at(Tok::Eof);
    }

    std::vector<StmtPtr> parse_block() {
        std::vector<StmtPtr> body;
        while (!block_terminator()) body.push_back(parse_stmt());
        return body;
    }

    StmtPtr parse_stmt() {
        switch (peek().kind) {
            case Tok::KwIf: return parse_if();
            case Tok::KwWhile: return parse_while();
            case Tok::KwRepeat: return parse_repeat();
            case Tok::KwFunc: return parse_func();
            case Tok::KwReturn: return parse_return();
            default: break;
        }
        if (at(Tok::Ident) && peek(1).kind == Tok::Assign) {
            auto s = mkstmt(StmtKind::Assign);
            s->name = advance().text;  // ident
            advance();                 // =
            s->expr = parse_expr();
            return s;
        }
        auto s = mkstmt(StmtKind::ExprStmt);
        s->expr = parse_expr();
        return s;
    }

    StmtPtr parse_if() {
        auto s = mkstmt(StmtKind::If);
        expect(Tok::KwIf, "'if'");
        s->expr = parse_expr();
        expect(Tok::KwThen, "'then'");
        s->body = parse_block();
        if (at(Tok::KwElse)) {
            advance();
            s->else_body = parse_block();
        }
        expect(Tok::KwEnd, "'end'");
        return s;
    }

    StmtPtr parse_while() {
        auto s = mkstmt(StmtKind::While);
        expect(Tok::KwWhile, "'while'");
        s->expr = parse_expr();
        expect(Tok::KwDo, "'do'");
        s->body = parse_block();
        expect(Tok::KwEnd, "'end'");
        return s;
    }

    StmtPtr parse_repeat() {
        auto s = mkstmt(StmtKind::Repeat);
        expect(Tok::KwRepeat, "'repeat'");
        s->expr = parse_expr();  // count
        expect(Tok::KwDo, "'do'");
        s->body = parse_block();
        expect(Tok::KwEnd, "'end'");
        return s;
    }

    StmtPtr parse_func() {
        auto s = mkstmt(StmtKind::FuncDecl);
        expect(Tok::KwFunc, "'func'");
        s->name = expect(Tok::Ident, "function name").text;
        expect(Tok::LParen, "'('");
        if (!at(Tok::RParen)) {
            s->params.push_back(expect(Tok::Ident, "parameter").text);
            while (at(Tok::Comma)) {
                advance();
                s->params.push_back(expect(Tok::Ident, "parameter").text);
            }
        }
        expect(Tok::RParen, "')'");
        s->body = parse_block();
        expect(Tok::KwEnd, "'end'");
        return s;
    }

    StmtPtr parse_return() {
        auto s = mkstmt(StmtKind::Return);
        expect(Tok::KwReturn, "'return'");
        if (!block_terminator()) {
            s->expr = parse_expr();
            s->has_value = true;
        }
        return s;
    }

    Program parse_program() {
        Program p;
        while (!at(Tok::Eof)) p.stmts.push_back(parse_stmt());
        return p;
    }
};

}  // namespace

Program parse(const std::string& src) {
    Parser p;
    p.toks = lex(src);
    return p.parse_program();
}

}  // namespace farm::lang
