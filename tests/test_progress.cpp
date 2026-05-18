#include <doctest/doctest.h>

#include "lang/parser.hpp"
#include "sim/progress.hpp"
#include "sim/robot_host.hpp"
#include "vm/vm.hpp"

using namespace farm;

TEST_CASE("progress: locked language features are rejected at parse") {
    sim::Progression p;  // nothing unlocked
    lang::FeatureSet fs = p.feature_set();

    CHECK_THROWS_AS(lang::parse("while true do wait() end", fs),
                    lang::ParseError);
    CHECK_THROWS_AS(lang::parse("if a then b() end", fs), lang::ParseError);
    CHECK_THROWS_AS(lang::parse("func f() end", fs), lang::ParseError);
    CHECK_THROWS_AS(lang::parse("repeat 3 do wait() end", fs),
                    lang::ParseError);
    // base language (assignment / native calls / return) always works
    CHECK_NOTHROW(lang::parse("x = 1\ntill()\nreturn x", fs));
}

TEST_CASE("progress: unlocking a feature enables it in subsequent parse") {
    sim::Progression prog;
    CHECK_THROWS_AS(lang::parse("while false do wait() end",
                                prog.feature_set()),
                    lang::ParseError);
    prog.grant(sim::U_Loops);
    CHECK_NOTHROW(lang::parse("while false do wait() end",
                              prog.feature_set()));
    // a different feature stays locked
    CHECK_THROWS_AS(lang::parse("if true then return 1 end",
                                prog.feature_set()),
                    lang::ParseError);
}

TEST_CASE("progress: economy — unlock spends wheat, then carrots work") {
    sim::World w(4, 4);
    sim::Progression prog;
    sim::RobotHost host(w, 1'000'000, &prog);
    lang::FeatureSet fs = prog.feature_set();  // base only

    // Base-language straight-line program (no loops/functions): grow and
    // harvest one wheat. Wheat ripens in 5 ticks.
    const char* grow_wheat =
        "till()\nplant(1)\n"
        "wait()\nwait()\nwait()\nwait()\nwait()\n"
        "harvest()";
    for (int k = 0; k < 6; ++k)
        vm::execute(lang::parse(grow_wheat, fs), host);
    CHECK(w.inventory_of(sim::CropWheat) == 6);

    // Carrot crop is locked -> planting id 2 does nothing.
    CHECK_FALSE(prog.crop_carrot_unlocked());

    // Unlock Carrot (id 4, cost 6 wheat) from base code via the native.
    lang::Value ok =
        vm::execute(lang::parse("return unlock(4)", fs), host);
    CHECK(ok == lang::Value::B(true));
    CHECK(prog.crop_carrot_unlocked());
    CHECK(w.inventory_of(sim::CropWheat) == 0);  // wheat was spent

    // Now a carrot (id 2, ripens in 8) can be grown & harvested.
    const char* grow_carrot =
        "till()\nplant(2)\n"
        "wait()\nwait()\nwait()\nwait()\nwait()\nwait()\nwait()\nwait()\n"
        "harvest()\nreturn num_items(2)";
    lang::Value cv = vm::execute(lang::parse(grow_carrot, fs), host);
    CHECK(cv == lang::Value::I(1));

    // Unlock fails when wheat is insufficient.
    lang::Value bad =
        vm::execute(lang::parse("return unlock(1)", fs), host);  // Loops=5
    CHECK(bad == lang::Value::B(false));
    CHECK_FALSE(prog.is_unlocked(sim::U_Loops));
}

TEST_CASE("progress: get_cost / num_unlocked natives") {
    sim::World w(4, 4);
    sim::Progression prog;
    sim::RobotHost host(w, 1'000'000, &prog);
    lang::FeatureSet fs = prog.feature_set();

    CHECK(vm::execute(lang::parse("return get_cost(1)", fs), host) ==
          lang::Value::I(sim::Progression::def(sim::U_Loops).cost_wheat));
    CHECK(vm::execute(lang::parse("return num_unlocked(1)", fs), host) ==
          lang::Value::I(0));
    prog.grant(sim::U_Loops);
    CHECK(vm::execute(lang::parse("return num_unlocked(1)", fs), host) ==
          lang::Value::I(1));
}

TEST_CASE("progress: max farm size grows with Expand unlock") {
    sim::Progression prog;
    CHECK(prog.max_farm_size() == 4);
    prog.grant(sim::U_Expand);
    CHECK(prog.max_farm_size() == 6);
}
