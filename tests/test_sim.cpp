#include <doctest/doctest.h>

#include "lang/interpreter.hpp"
#include "lang/parser.hpp"
#include "sim/robot_host.hpp"
#include "vm/vm.hpp"

using namespace farm;

TEST_CASE("sim: sense progresses wild->tilled->growing->mature->wild") {
    const char* src =
        "till()\n"
        "s1 = sense()\n"          // tilled empty -> 1
        "plant(1)\n"
        "s2 = sense()\n"          // growing -> 2
        "repeat 4 do wait() end\n"  // plant=>age1, +4 => age5 (mature)
        "s3 = sense()\n"          // mature -> 3
        "harvest()\n"
        "s4 = sense()\n"          // crop gone, soil reset -> wild 0
        "return s1 * 1000 + s2 * 100 + s3 * 10 + s4";

    sim::World w(1, 1);
    sim::RobotHost host(w);
    lang::Value r = vm::execute(src, host);
    CHECK(r == lang::Value::I(1230));
    CHECK(w.inventory_of(sim::CropWheat) == 1);
}

TEST_CASE("sim: golden farm program — robot fills a 3-tile row") {
    const char* src =
        "func grow_one()\n"
        "  till()\n"
        "  plant(1)\n"
        "  repeat 5 do water() end\n"
        "  harvest()\n"
        "end\n"
        "i = 0\n"
        "while i < 3 do\n"
        "  grow_one()\n"
        "  move(1)\n"            // East
        "  i = i + 1\n"
        "end\n"
        "return inventory(1)";

    sim::World w(3, 1);
    sim::RobotHost host(w);
    lang::Value r = vm::execute(src, host);

    CHECK(r == lang::Value::I(3));
    CHECK(w.inventory_of(sim::CropWheat) == 3);
    CHECK(w.rx() == 2);   // last move East off-grid was blocked
    CHECK(w.ry() == 0);
    CHECK(w.tick() > 0);
    // every worked tile was harvested clean
    for (int x = 0; x < 3; ++x) {
        CHECK(w.tile(x, 0).crop == sim::CropNone);
        CHECK_FALSE(w.tile(x, 0).tilled);
    }
}

TEST_CASE("sim: VM and interpreter drive the world identically") {
    const char* src =
        "till() plant(1)\n"
        "while can_harvest() == false do wait() end\n"
        "harvest()\n"
        "return inventory(1)";

    sim::World wv(1, 1), wi(1, 1);
    sim::RobotHost hv(wv), hi(wi);
    lang::Program ast = lang::parse(src);

    lang::Value rv = vm::execute(ast, hv);
    lang::Value ri = lang::interpret(ast, hi);

    CHECK(rv == ri);
    CHECK(rv == lang::Value::I(1));
    CHECK(wv.tick() == wi.tick());
    CHECK(wv.inventory_of(sim::CropWheat) ==
          wi.inventory_of(sim::CropWheat));
}

TEST_CASE("sim: bad robot calls raise RuntimeError") {
    sim::World w(2, 2);
    sim::RobotHost host(w);
    CHECK_THROWS_AS(vm::execute("move()", host), lang::RuntimeError);
    CHECK_THROWS_AS(vm::execute("teleport()", host), lang::RuntimeError);
    CHECK_THROWS_AS(vm::execute("plant(true)", host), lang::RuntimeError);
}
