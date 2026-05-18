#include <doctest/doctest.h>

#include "lang/parser.hpp"
#include "sim/progress.hpp"
#include "sim/robot_host.hpp"
#include "sim/world.hpp"
#include "vm/vm.hpp"

using namespace farm;

TEST_CASE("enhance: watering doubles growth speed") {
    sim::World w(1, 1);
    w.till();
    w.plant(sim::CropWheat);  // maturity 5
    w.water();

    SUBCASE("watering disabled -> 1/tick") {
        w.set_watering(false);
        for (int i = 0; i < 4; ++i) w.advance_tick();
        CHECK_FALSE(w.can_harvest());  // age 4 < 5
        w.advance_tick();
        CHECK(w.can_harvest());        // age 5
    }
    SUBCASE("watering enabled -> 2/tick on watered tile") {
        w.set_watering(true);
        w.advance_tick();
        w.advance_tick();
        w.advance_tick();              // age 6 -> clamped to 5
        CHECK(w.can_harvest());
    }
}

TEST_CASE("enhance: fertilize instantly ripens") {
    sim::World w(1, 1);
    w.till();
    w.plant(sim::CropWheat);
    CHECK_FALSE(w.can_harvest());
    CHECK(w.fertilize());
    CHECK(w.can_harvest());
    CHECK(w.harvest());
    CHECK(w.inventory_of(sim::CropWheat) == 1);
}

TEST_CASE("enhance: pumpkin is slow but high yield") {
    CHECK(sim::crop_maturity(sim::CropPumpkin) == 12);
    CHECK(sim::crop_base_yield(sim::CropPumpkin) == 3);

    sim::World w(1, 1);
    w.till();
    w.plant(sim::CropPumpkin);
    w.fertilize();
    CHECK(w.harvest());
    CHECK(w.inventory_of(sim::CropPumpkin) == 3);
}

TEST_CASE("enhance: polyculture gives a companion bonus") {
    sim::World w(2, 1);  // (0,0) carrot, (1,0) wheat
    // plant a carrot at (0,0) and leave it
    w.till();
    w.plant(sim::CropCarrot);
    // move east, grow wheat next to the carrot
    w.move(sim::East);
    w.till();
    w.plant(sim::CropWheat);
    w.fertilize();

    SUBCASE("no polyculture -> base yield") {
        w.set_polyculture(false);
        CHECK(w.harvest());
        CHECK(w.inventory_of(sim::CropWheat) == 1);
    }
    SUBCASE("polyculture -> +1 next to companion (carrot)") {
        w.set_polyculture(true);
        CHECK(w.harvest());
        CHECK(w.inventory_of(sim::CropWheat) == 2);
    }
}

TEST_CASE("enhance: unlock gating via RobotHost") {
    sim::World w(2, 2);
    sim::Progression prog;
    sim::RobotHost host(w, 1'000'000, &prog);
    lang::FeatureSet fs = prog.feature_set();

    // fertilize locked at start
    CHECK(vm::execute(lang::parse("till() plant(1)\nreturn fertilize()", fs),
                      host) == lang::Value::B(false));

    // pumpkin (id 3) locked -> plant does nothing
    CHECK(vm::execute(lang::parse("till()\nreturn plant(3)", fs), host) ==
          lang::Value::B(false));

    prog.grant(sim::U_Fertilizer);
    prog.grant(sim::U_Pumpkin);
    sim::World w2(2, 2);
    sim::RobotHost host2(w2, 1'000'000, &prog);
    CHECK(vm::execute(
              lang::parse("till() plant(3) fertilize()\n"
                          "harvest()\nreturn num_items(3)",
                          fs),
              host2) == lang::Value::I(3));
}

TEST_CASE("enhance: watering unlock changes behaviour through the host") {
    const char* prog_src =
        "till() plant(1) water() wait()\nreturn can_harvest()";

    sim::Progression locked;  // no Watering
    sim::World w1(1, 1);
    sim::RobotHost h1(w1, 1'000'000, &locked);
    CHECK(vm::execute(lang::parse(prog_src, locked.feature_set()), h1) ==
          lang::Value::B(false));

    sim::Progression unlocked;
    unlocked.grant(sim::U_Watering);
    sim::World w2(1, 1);
    sim::RobotHost h2(w2, 1'000'000, &unlocked);
    CHECK(vm::execute(lang::parse(prog_src, unlocked.feature_set()), h2) ==
          lang::Value::B(true));
}
