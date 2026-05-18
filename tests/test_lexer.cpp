#include <doctest/doctest.h>

#include "lang/lexer.hpp"

using namespace farm::lang;

namespace {
std::vector<Tok> kinds(const std::string& s) {
    std::vector<Tok> k;
    for (auto& t : lex(s)) k.push_back(t.kind);
    return k;
}
}  // namespace

TEST_CASE("lexer: integers and identifiers") {
    auto t = lex("x = 42");
    REQUIRE(t.size() == 4);  // ident '=' int eof
    CHECK(t[0].kind == Tok::Ident);
    CHECK(t[0].text == "x");
    CHECK(t[1].kind == Tok::Assign);
    CHECK(t[2].kind == Tok::Int);
    CHECK(t[2].ivalue == 42);
    CHECK(t[3].kind == Tok::Eof);
}

TEST_CASE("lexer: keywords vs identifiers") {
    auto t = lex("if iffy then end");
    CHECK(t[0].kind == Tok::KwIf);
    CHECK(t[1].kind == Tok::Ident);  // 'iffy' is not a keyword
    CHECK(t[1].text == "iffy");
    CHECK(t[2].kind == Tok::KwThen);
    CHECK(t[3].kind == Tok::KwEnd);
}

TEST_CASE("lexer: two-char operators") {
    CHECK(kinds("== != <= >= < > =") ==
          std::vector<Tok>{Tok::Eq, Tok::Ne, Tok::Le, Tok::Ge, Tok::Lt,
                           Tok::Gt, Tok::Assign, Tok::Eof});
}

TEST_CASE("lexer: comments and newlines are skipped") {
    auto t = lex("# comment\n  a # trailing\n+1");
    CHECK(kinds("# c\n a\n+1") ==
          std::vector<Tok>{Tok::Ident, Tok::Plus, Tok::Int, Tok::Eof});
    CHECK(t[0].line == 2);  // 'a' is on line 2
}

TEST_CASE("lexer: line/col tracking") {
    auto t = lex("a\n  bb");
    CHECK(t[0].line == 1);
    CHECK(t[0].col == 1);
    CHECK(t[1].line == 2);
    CHECK(t[1].col == 3);
}

TEST_CASE("lexer: errors") {
    CHECK_THROWS_AS(lex("a $ b"), LexError);
    CHECK_THROWS_AS(lex("a ! b"), LexError);  // lone '!'
    CHECK_THROWS_AS(lex("99999999999999999999999999"), LexError);  // overflow
}
