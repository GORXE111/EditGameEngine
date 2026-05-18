#pragma once
#include "lang/value.hpp"
#include "sim/world.hpp"

namespace farm::sim {

// Binds the FarmCode robot API to a World. Tick-consuming actions
// (move/till/plant/water/harvest/wait) advance the world one tick before
// returning — this is how "an action takes time" while crops grow. Query
// actions (sense/pos/inventory/can_harvest) are free.
//
// `max_ticks` guards headless runs from never-terminating programs.
class RobotHost : public farm::lang::Host {
public:
    explicit RobotHost(World& w, uint64_t max_ticks = 1'000'000)
        : world_(w), max_ticks_(max_ticks) {}

    farm::lang::Value call(
        const std::string& name,
        const std::vector<farm::lang::Value>& args) override;

    World& world() { return world_; }

private:
    int arg_int(const std::vector<farm::lang::Value>& a, size_t i,
                const char* fn);
    void consume_tick();

    World& world_;
    uint64_t max_ticks_;
};

}  // namespace farm::sim
