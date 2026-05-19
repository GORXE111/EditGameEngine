#include "sim/fleet.hpp"

#include <memory>

#include "sim/robot_host.hpp"
#include "vm/compiler.hpp"
#include "vm/vm.hpp"

namespace farm::sim {

FleetResult run_fleet(const farm::lang::Program& program, World& w,
                       Progression* prog, int ndrones, int max_rounds) {
    if (ndrones < 1) ndrones = 1;
    vm::Chunk chunk = vm::compile(program);
    while (w.drones() < ndrones) w.add_drone();

    std::vector<std::unique_ptr<RobotHost>> hosts;
    std::vector<std::unique_ptr<vm::VM>> vms;
    for (int i = 0; i < ndrones; ++i) {
        hosts.push_back(std::make_unique<RobotHost>(
            w, /*max_ticks*/ 1'000'000'000ull, prog, i,
            /*auto_tick*/ false));
        vms.push_back(std::make_unique<vm::VM>(chunk, *hosts[i]));
    }

    FleetResult res;
    res.finished.assign(ndrones, false);

    for (int round = 0; round < max_rounds; ++round) {
        bool any_live = false;
        for (int i = 0; i < ndrones; ++i) {
            if (vms[i]->finished()) continue;
            any_live = true;
            uint64_t a0 = hosts[i]->actions();
            // single-step until this drone performs one timed action
            // (or its program ends)
            for (int guard = 0;
                 !vms[i]->finished() && hosts[i]->actions() == a0 &&
                 guard < 500000;
                 ++guard)
                vms[i]->run(1);
        }
        if (!any_live) break;
        w.advance_tick();  // one shared tick per round
        res.rounds = round + 1;
    }

    for (int i = 0; i < ndrones; ++i)
        res.finished[i] = vms[i]->finished();
    return res;
}

}  // namespace farm::sim
