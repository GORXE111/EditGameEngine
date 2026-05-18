#include <doctest/doctest.h>

#include "blueprint/codec.hpp"
#include "lang/parser.hpp"
#include "lang/printer.hpp"

using namespace farm;

namespace {
int find_kind(const blueprint::Graph& g, blueprint::NK k, int nth = 0) {
    int seen = 0;
    for (size_t i = 0; i < g.nodes.size(); ++i)
        if (g.nodes[i].kind == k && seen++ == nth) return (int)i;
    return -1;
}
}  // namespace

TEST_CASE("sync: graph -> AST -> graph -> AST is stable") {
    const char* src =
        "func f(n) if n < 2 then return n end return f(n-1)+f(n-2) end\n"
        "i = 0\n"
        "while i < 5 do i = i + 1 end\n"
        "return f(i)";
    lang::Program a0 = lang::parse(src);
    std::string p0 = lang::print(a0);

    lang::Program a1 = blueprint::to_ast(blueprint::build(a0));
    lang::Program a2 = blueprint::to_ast(blueprint::build(a1));
    CHECK(lang::print(a1) == p0);
    CHECK(lang::print(a2) == p0);
}

TEST_CASE("sync: blueprint layout survives a re-derive (keyed by AST id)") {
    const char* src1 = "a = 1\nb = a + 2\nreturn b";
    blueprint::Graph g1 = blueprint::build(lang::parse(src1));

    // simulate the user dragging the 'b = ...' Assign node somewhere
    int bidx = find_kind(g1, blueprint::NK::Assign, 1);  // 2nd Assign
    REQUIRE(bidx >= 0);
    lang::NodeId bid = g1.nodes[bidx].ast_id;
    g1.nodes[bidx].x = 1234.f;
    g1.nodes[bidx].y = 567.f;
    blueprint::LayoutStore ls = blueprint::capture_layout(g1);

    // append a new statement; earlier nodes keep their ids -> keep layout
    const char* src2 = "a = 1\nb = a + 2\nreturn b\nc = 9";
    blueprint::Graph g2 = blueprint::build(lang::parse(src2), ls);

    int found = -1;
    for (size_t i = 0; i < g2.nodes.size(); ++i)
        if (g2.nodes[i].ast_id == bid) { found = (int)i; break; }
    REQUIRE(found >= 0);
    CHECK(g2.nodes[found].x == doctest::Approx(1234.f));
    CHECK(g2.nodes[found].y == doctest::Approx(567.f));
}

TEST_CASE("sync: editing a blueprint node propagates to code") {
    blueprint::Graph g = blueprint::build(lang::parse("x = 1\nreturn x + 0"));

    SUBCASE("change an int literal") {
        int lit = find_kind(g, blueprint::NK::IntLit, 0);  // the '1'
        REQUIRE(lit >= 0);
        g.nodes[lit].ival = 42;
        std::string code = lang::print(blueprint::to_ast(g));
        CHECK(code.find("x = 42") != std::string::npos);
        // and it re-parses + round-trips stably
        CHECK(lang::print(blueprint::to_ast(
                  blueprint::build(lang::parse(code)))) == code);
    }
    SUBCASE("rename a variable get") {
        int v = find_kind(g, blueprint::NK::VarGet, 0);  // 'x' in x + 0
        REQUIRE(v >= 0);
        g.nodes[v].name = "y";
        std::string code = lang::print(blueprint::to_ast(g));
        CHECK(code.find("return y + 0") != std::string::npos);
    }
    SUBCASE("change a binary operator") {
        int b = find_kind(g, blueprint::NK::Binary, 0);  // x + 0
        REQUIRE(b >= 0);
        g.nodes[b].op = lang::Tok::Star;
        std::string code = lang::print(blueprint::to_ast(g));
        CHECK(code.find("return x * 0") != std::string::npos);
    }
}
