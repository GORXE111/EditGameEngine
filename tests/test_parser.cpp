#include <doctest/doctest.h>

#include "lang/parser.hpp"
#include "lang/printer.hpp"

using namespace farm::lang;

static std::string canon(const std::string& src) {
    return print(parse(src));
}

TEST_CASE("parser: node ids are assigned and unique-ish") {
    Program p = parse("x = 1\ny = x + 2");
    REQUIRE(p.stmts.size() == 2);
    CHECK(p.stmts[0]->id != kNoNode);
    CHECK(p.stmts[1]->id != kNoNode);
    CHECK(p.stmts[0]->id != p.stmts[1]->id);
    CHECK(p.stmts[0]->expr->id != kNoNode);
}

TEST_CASE("printer: arithmetic precedence is preserved minimally") {
    CHECK(canon("x = 1 + 2 * 3") == "x = 1 + 2 * 3\n");
    CHECK(canon("x = (1 + 2) * 3") == "x = (1 + 2) * 3\n");
    CHECK(canon("x = 1 - 2 - 3") == "x = 1 - 2 - 3\n");      // left assoc
    CHECK(canon("x = 1 - (2 - 3)") == "x = 1 - (2 - 3)\n");  // keep parens
    CHECK(canon("x = not a and b") == "x = not a and b\n");
    CHECK(canon("x = not (a and b)") == "x = not (a and b)\n");
    CHECK(canon("x = -a + b") == "x = -a + b\n");
    CHECK(canon("x = -(a + b)") == "x = -(a + b)\n");
}

TEST_CASE("printer: control flow canonical form") {
    const char* src = "if a then b=1 else b=2 end";
    CHECK(canon(src) ==
          "if a then\n"
          "  b = 1\n"
          "else\n"
          "  b = 2\n"
          "end\n");

    CHECK(canon("while x < 10 do x = x + 1 end") ==
          "while x < 10 do\n"
          "  x = x + 1\n"
          "end\n");

    CHECK(canon("repeat 3 do step() end") ==
          "repeat 3 do\n"
          "  step()\n"
          "end\n");
}

TEST_CASE("printer: functions, calls, return") {
    const char* src =
        "func add(a, b) return a + b end\n"
        "r = add(1, 2)\n"
        "return r";
    CHECK(canon(src) ==
          "func add(a, b)\n"
          "  return a + b\n"
          "end\n"
          "r = add(1, 2)\n"
          "return r\n");
}

TEST_CASE("printer: round-trip is idempotent") {
    const char* progs[] = {
        "x = 1 + 2 * (3 - 4) % 5",
        "if a and not b then c() else d() end",
        "func f(n) if n < 2 then return n end return f(n - 1) + f(n - 2) end",
        "while true do repeat 2 do tick() end end",
    };
    for (auto* s : progs) {
        std::string c1 = canon(s);
        std::string c2 = print(parse(c1));  // re-parse canonical
        CHECK(c1 == c2);
    }
}

TEST_CASE("parser: syntax errors") {
    CHECK_THROWS_AS(parse("if a then b end end"), ParseError);  // extra end
    CHECK_THROWS_AS(parse("x = "), ParseError);                 // no rhs
    CHECK_THROWS_AS(parse("func (a) end"), ParseError);         // no name
    CHECK_THROWS_AS(parse("while x x end"), ParseError);        // missing do
}
