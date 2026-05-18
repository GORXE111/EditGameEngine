#include <doctest/doctest.h>

#include "blueprint/codec.hpp"
#include "lang/interpreter.hpp"
#include "lang/parser.hpp"
#include "lang/printer.hpp"
#include "sim/robot_host.hpp"
#include "vm/vm.hpp"

using namespace farm;

namespace {
const char* kProgs[] = {
    "x = 1 + 2 * (3 - 4) % 5\nreturn -x",
    "y = not (1 < 2) or 3 == 3 and false\nreturn y",
    "if a and not b then c = 1 else c = 2 end\nreturn c",
    "i = 0 s = 0\nwhile i < 10 do s = s + i i = i + 1 end\nreturn s",
    "n = 0\nrepeat 5 do n = n + 1 if n == 3 then n = n + 10 end end\n"
    "return n",
    "func fib(n) if n < 2 then return n end "
    "return fib(n-1) + fib(n-2) end\nreturn fib(12)",
    "func greet(a, b) emit(a) emit(b) end\ngreet(1, 2)\nreturn 0",
    "till() plant(1)\nwhile can_harvest() == false do wait() end\n"
    "harvest()\nreturn inventory(1)",
};
}  // namespace

TEST_CASE("blueprint: AST -> graph -> AST preserves the program") {
    for (auto* src : kProgs) {
        lang::Program a1 = lang::parse(src);
        blueprint::Graph g = blueprint::build(a1);
        lang::Program a2 = blueprint::to_ast(g);

        // canonical printer is idempotent (M1.2) -> equality == structural eq
        CHECK(lang::print(a1) == lang::print(a2));
        // idempotent through a second round-trip
        CHECK(lang::print(blueprint::to_ast(blueprint::build(a2))) ==
              lang::print(a1));
    }
}

TEST_CASE("blueprint: graph nodes carry AST ids; entry present") {
    blueprint::Graph g = blueprint::build(lang::parse("x = 1\nreturn x + 2"));
    REQUIRE(g.entry >= 0);
    CHECK(g.nodes[g.entry].kind == blueprint::NK::Entry);
    int real = 0, with_id = 0;
    for (auto& n : g.nodes) {
        if (n.kind == blueprint::NK::Entry) continue;
        ++real;
        if (n.ast_id != lang::kNoNode) ++with_id;
    }
    CHECK(real > 0);
    CHECK(with_id == real);  // every mapped node keeps its AST NodeId
}

TEST_CASE("blueprint: graph-derived program runs identically (sim)") {
    const char* src =
        "func grow()\n"
        "  till() plant(1)\n"
        "  repeat 5 do water() end\n"
        "  harvest()\n"
        "end\n"
        "i = 0\n"
        "while i < 3 do grow() move(1) i = i + 1 end\n"
        "return inventory(1)";

    lang::Program a1 = lang::parse(src);
    lang::Program a2 = blueprint::to_ast(blueprint::build(a1));

    sim::World w1(3, 1), w2(3, 1);
    sim::RobotHost h1(w1), h2(w2);
    lang::Value r1 = vm::execute(a1, h1);
    lang::Value r2 = vm::execute(a2, h2);

    CHECK(r1 == r2);
    CHECK(r2 == lang::Value::I(3));
    CHECK(w1.inventory_of(sim::CropWheat) ==
          w2.inventory_of(sim::CropWheat));
    CHECK(w1.tick() == w2.tick());
}
