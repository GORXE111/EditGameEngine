#include <doctest/doctest.h>

#include "lang/parser.hpp"
#include "sim/fleet.hpp"
#include "sim/world.hpp"

using namespace farm;

TEST_CASE("fleet: shared program, drones split work by id") {
    const char* src =
        "if get_drone_id() == 0 then\n"
        "  till() plant(1)\n"
        "  wait() wait() wait() wait() wait()\n"
        "  harvest()\n"
        "else\n"
        "  move(1)\n"                       // drone 1 works the next tile
        "  till() plant(1)\n"
        "  wait() wait() wait() wait() wait()\n"
        "  harvest()\n"
        "end\n"
        "return 0";

    sim::World w(3, 3);
    sim::FleetResult r =
        sim::run_fleet(lang::parse(src), w, nullptr, 2);

    CHECK(w.drones() == 2);
    CHECK(r.finished[0]);
    CHECK(r.finished[1]);
    CHECK(w.inventory_of(sim::CropWheat) == 2);  // each drone harvested 1
    CHECK(w.rx() == 0);          // drone 0 stayed at origin
    CHECK(w.drone_x(1) == 1);    // drone 1 moved east
}

TEST_CASE("fleet: get_drone_id distinguishes drones") {
    const char* src =
        "i = 0\n"
        "while i < get_drone_id() do move(1) i = i + 1 end\n"
        "return 0";

    sim::World w(5, 5);
    sim::run_fleet(lang::parse(src), w, nullptr, 3);

    CHECK(w.drones() == 3);
    CHECK(w.drone_x(0) == 0);
    CHECK(w.drone_x(1) == 1);
    CHECK(w.drone_x(2) == 2);
}

TEST_CASE("fleet: num_drones native reports the fleet size") {
    sim::World w(3, 3);
    sim::run_fleet(lang::parse("return num_drones()"), w, nullptr, 4);
    CHECK(w.drones() == 4);
}

TEST_CASE("fleet: one drone matches single-drone semantics") {
    const char* src =
        "till() plant(1)\n"
        "wait() wait() wait() wait() wait()\n"
        "harvest()\nreturn 0";
    sim::World w(2, 2);
    sim::FleetResult r = sim::run_fleet(lang::parse(src), w, nullptr, 1);
    CHECK(w.drones() == 1);
    CHECK(r.finished[0]);
    CHECK(w.inventory_of(sim::CropWheat) == 1);
}

TEST_CASE("fleet: progression gates still apply per drone") {
    sim::World w(3, 3);
    sim::Progression prog;  // carrot locked
    // drones work separate tiles (offset by id) and grow a carrot;
    // locked -> nothing harvested.
    const char* src =
        "move(get_drone_id())\n"   // drone 1 -> east; drone 0 stays
        "till() plant(2)\n"
        "wait() wait() wait() wait()\n"
        "wait() wait() wait() wait()\n"
        "harvest()\nreturn 0";
    sim::run_fleet(lang::parse(src), w, &prog, 2);
    CHECK(w.inventory_of(sim::CropCarrot) == 0);

    sim::World w2(3, 3);
    sim::Progression prog2;
    prog2.grant(sim::U_Carrot);
    sim::run_fleet(lang::parse(src), w2, &prog2, 2);
    CHECK(w2.inventory_of(sim::CropCarrot) == 2);  // both drones, gated open
}
