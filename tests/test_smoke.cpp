#include <doctest/doctest.h>

#include "lang/lang.hpp"
#include "vm/vm.hpp"
#include "sim/sim.hpp"
#include "blueprint/blueprint.hpp"

// Headless link smoke test (from M0).
TEST_CASE("modules link") {
    CHECK(farm::lang::version() == "lang-v0");
    CHECK(farm::vm::version() == "vm-v0");
    CHECK(farm::sim::version() == "sim-v0");
    CHECK(farm::blueprint::version() == "blueprint-v0");
}
