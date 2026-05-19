#pragma once
#include <vector>

#include "lang/ast.hpp"
#include "sim/progress.hpp"
#include "sim/world.hpp"

// Shared-program multi-drone runner. One compiled program, N drones, each
// with its own VM instance over the same bytecode (distinguished at runtime
// via get_drone_id()). Round-robin: every round each live drone performs
// exactly one tick-consuming action, then the world advances one tick — so
// crops grow once per shared tick while drones act in parallel.
namespace farm::sim {

struct FleetResult {
    int rounds = 0;
    std::vector<bool> finished;  // per drone
};

FleetResult run_fleet(const farm::lang::Program& program, World& w,
                       Progression* prog, int ndrones,
                       int max_rounds = 200000);

}  // namespace farm::sim
