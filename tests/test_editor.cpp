#include <doctest/doctest.h>

#include "blueprint/codec.hpp"
#include "lang/printer.hpp"
#include "lang/value.hpp"

using namespace farm;

TEST_CASE("editor: spawn assigns default arity and -1 sentinels") {
    blueprint::Graph g;
    int e = blueprint::spawn(g, blueprint::NK::Entry);
    CHECK(e == 0);
    CHECK(g.nodes[e].in.empty());

    int bin = blueprint::spawn(g, blueprint::NK::Binary);
    REQUIRE(g.nodes[bin].in.size() == 2);
    CHECK(g.nodes[bin].in[0] == -1);
    CHECK(g.nodes[bin].in[1] == -1);

    int set_ix = blueprint::spawn(g, blueprint::NK::SetIndex);
    CHECK(g.nodes[set_ix].in.size() == 3);
}

TEST_CASE("editor: to_ast throws a helpful error on unwired pins") {
    // Hand-build a graph: Entry -> If (cond unwired) -> nothing.
    blueprint::Graph g;
    int e  = blueprint::spawn(g, blueprint::NK::Entry);
    g.entry = e;
    int ifn = blueprint::spawn(g, blueprint::NK::If);  // cond pin = -1
    g.nodes[e].exec_next = ifn;

    CHECK_THROWS_AS(blueprint::to_ast(g), lang::RuntimeError);
}

TEST_CASE("editor: to_ast throws when an exec chain reaches a deleted node") {
    blueprint::Graph g;
    int e   = blueprint::spawn(g, blueprint::NK::Entry);
    g.entry = e;
    int ret = blueprint::spawn(g, blueprint::NK::Return);
    g.nodes[ret].in[0] = -1;        // value pin empty (so has_value=false)
    g.nodes[e].exec_next = ret;
    g.nodes[ret].deleted = true;    // stale wire
    CHECK_THROWS_AS(blueprint::to_ast(g), lang::RuntimeError);
}

TEST_CASE("editor: compact drops tombstones and remaps indices") {
    // entry -> A (will be deleted) -> B
    blueprint::Graph g;
    int e = blueprint::spawn(g, blueprint::NK::Entry);
    g.entry = e;
    int a = blueprint::spawn(g, blueprint::NK::CallStmt);
    g.nodes[a].name = "till";
    int b = blueprint::spawn(g, blueprint::NK::CallStmt);
    g.nodes[b].name = "wait";
    g.nodes[e].exec_next = a;
    g.nodes[a].exec_next = b;

    // delete A and clear inbound refs (what the editor does on Delete)
    g.nodes[a].deleted = true;
    g.nodes[e].exec_next = b;       // re-wire skip-over

    blueprint::compact(g);
    REQUIRE(g.nodes.size() == 2);   // entry + B
    CHECK(g.nodes[0].kind == blueprint::NK::Entry);
    CHECK(g.nodes[1].kind == blueprint::NK::CallStmt);
    CHECK(g.nodes[1].name == "wait");
    CHECK(g.nodes[0].exec_next == 1);  // remap of old idx 2 -> new 1

    // and the resulting program is valid
    lang::Program p = blueprint::to_ast(g);
    CHECK(lang::print(p) == "wait()\n");
}

TEST_CASE("editor: hand-built graph compiles, prints, and round-trips") {
    // Build:  Entry -> Move(direction=East=1) -> Return  (no return value)
    blueprint::Graph g;
    int e   = blueprint::spawn(g, blueprint::NK::Entry);
    g.entry = e;
    int mv  = blueprint::spawn(g, blueprint::NK::CallStmt);
    g.nodes[mv].name = "move";
    g.nodes[mv].in.assign(blueprint::native_arity("move"), -1);
    int lit = blueprint::spawn(g, blueprint::NK::IntLit);
    g.nodes[lit].ival = 1;
    g.nodes[mv].in[0] = lit;

    int ret = blueprint::spawn(g, blueprint::NK::Return);  // no value wired
    g.nodes[e].exec_next  = mv;
    g.nodes[mv].exec_next = ret;

    lang::Program p1 = blueprint::to_ast(g);
    std::string s1 = lang::print(p1);
    CHECK(s1.find("move(1)") != std::string::npos);
    CHECK(s1.find("return") != std::string::npos);

    // re-derive the graph from the printed text and ensure stability
    blueprint::Graph g2 = blueprint::build(p1);
    CHECK(lang::print(blueprint::to_ast(g2)) == s1);
}
