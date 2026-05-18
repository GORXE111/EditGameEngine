#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include "lang/lang.hpp"
#include "vm/vm.hpp"
#include "sim/sim.hpp"
#include "blueprint/blueprint.hpp"

// M0 smoke test: every headless-testable lib links and is reachable.
// Real golden tests (programs, round-trips) arrive in M1+.
TEST_CASE("M0 modules link") {
    CHECK(farm::lang::version() == "lang-v0");
    CHECK(farm::vm::version() == "vm-v0");
    CHECK(farm::sim::version() == "sim-v0");
    CHECK(farm::blueprint::version() == "blueprint-v0");
}
