#include "lang/lexer.hpp"

#include <cctype>
#include <limits>
#include <string_view>
#include <unordered_map>

namespace farm::lang {

const char* tok_name(Tok k) {
    switch (k) {
        case Tok::Int: return "int";
        case Tok::Ident: return "ident";
        case Tok::KwIf: return "if";
        case Tok::KwThen: return "then";
        case Tok::KwElse: return "else";
        case Tok::KwEnd: return "end";
        case Tok::KwWhile: return "while";
        case Tok::KwDo: return "do";
        case Tok::KwRepeat: return "repeat";
        case Tok::KwFunc: return "func";
        case Tok::KwReturn: return "return";
        case Tok::KwTrue: return "true";
        case Tok::KwFalse: return "false";
        case Tok::KwAnd: return "and";
        case Tok::KwOr: return "or";
        case Tok::KwNot: return "not";
        case Tok::LParen: return "(";
        case Tok::RParen: return ")";
        case Tok::Comma: return ",";
        case Tok::Assign: return "=";
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
        case Tok::Eof: return "<eof>";
    }
    return "<?>";
}

namespace {

const std::unordered_map<std::string_view, Tok>& keywords() {
    static const std::unordered_map<std::string_view, Tok> kw = {
        {"if", Tok::KwIf},       {"then", Tok::KwThen},
        {"else", Tok::KwElse},   {"end", Tok::KwEnd},
        {"while", Tok::KwWhile}, {"do", Tok::KwDo},
        {"repeat", Tok::KwRepeat}, {"func", Tok::KwFunc},
        {"return", Tok::KwReturn}, {"true", Tok::KwTrue},
        {"false", Tok::KwFalse}, {"and", Tok::KwAnd},
        {"or", Tok::KwOr},       {"not", Tok::KwNot},
    };
    return kw;
}

struct Cursor {
    const std::string& s;
    size_t i = 0;
    int line = 1, col = 1;

    bool eof() const { return i >= s.size(); }
    char peek() const { return eof() ? '\0' : s[i]; }
    char peek2() const { return (i + 1 < s.size()) ? s[i + 1] : '\0'; }
    char get() {
        char c = s[i++];
        if (c == '\n') { ++line; col = 1; } else { ++col; }
        return c;
    }
};

}  // namespace

std::vector<Token> lex(const std::string& src) {
    std::vector<Token> out;
    Cursor c{src};

    auto push = [&](Tok k, int l, int col, std::string text = {},
                     int64_t v = 0) {
        Token t;
        t.kind = k;
        t.text = std::move(text);
        t.ivalue = v;
        t.line = l;
        t.col = col;
        out.push_back(std::move(t));
    };

    while (!c.eof()) {
        char ch = c.peek();

        // whitespace
        if (ch == ' ' || ch == '\t' || ch == '\r' || ch == '\n') {
            c.get();
            continue;
        }
        // line comment
        if (ch == '#') {
            while (!c.eof() && c.peek() != '\n') c.get();
            continue;
        }

        int l = c.line, col = c.col;

        // integer literal
        if (std::isdigit(static_cast<unsigned char>(ch))) {
            int64_t v = 0;
            while (!c.eof() &&
                   std::isdigit(static_cast<unsigned char>(c.peek()))) {
                int d = c.get() - '0';
                if (v > (std::numeric_limits<int64_t>::max() - d) / 10)
                    throw LexError("integer literal overflow", l, col);
                v = v * 10 + d;
            }
            push(Tok::Int, l, col, {}, v);
            continue;
        }

        // identifier / keyword
        if (std::isalpha(static_cast<unsigned char>(ch)) || ch == '_') {
            std::string id;
            while (!c.eof()) {
                char p = c.peek();
                if (std::isalnum(static_cast<unsigned char>(p)) || p == '_')
                    id += c.get();
                else
                    break;
            }
            auto it = keywords().find(id);
            push(it != keywords().end() ? it->second : Tok::Ident, l, col,
                 std::move(id));
            continue;
        }

        // operators / punctuation
        c.get();
        switch (ch) {
            case '(': push(Tok::LParen, l, col); break;
            case ')': push(Tok::RParen, l, col); break;
            case ',': push(Tok::Comma, l, col); break;
            case '+': push(Tok::Plus, l, col); break;
            case '-': push(Tok::Minus, l, col); break;
            case '*': push(Tok::Star, l, col); break;
            case '/': push(Tok::Slash, l, col); break;
            case '%': push(Tok::Percent, l, col); break;
            case '=':
                if (c.peek() == '=') { c.get(); push(Tok::Eq, l, col); }
                else push(Tok::Assign, l, col);
                break;
            case '!':
                if (c.peek() == '=') { c.get(); push(Tok::Ne, l, col); }
                else throw LexError("unexpected '!' (did you mean '!=' ?)",
                                    l, col);
                break;
            case '<':
                if (c.peek() == '=') { c.get(); push(Tok::Le, l, col); }
                else push(Tok::Lt, l, col);
                break;
            case '>':
                if (c.peek() == '=') { c.get(); push(Tok::Ge, l, col); }
                else push(Tok::Gt, l, col);
                break;
            default:
                throw LexError(std::string("unexpected character '") + ch +
                                   "'",
                               l, col);
        }
    }

    push(Tok::Eof, c.line, c.col);
    return out;
}

}  // namespace farm::lang
