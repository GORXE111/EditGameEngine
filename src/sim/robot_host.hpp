#pragma once
#include "lang/value.hpp"
#include "sim/progress.hpp"
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
    explicit RobotHost(World& w, uint64_t max_ticks = 1'000'000,
                       Progression* prog = nullptr, int drone = 0,
                       bool auto_tick = true)
        : world_(w), max_ticks_(max_ticks), prog_(prog), drone_(drone),
          auto_tick_(auto_tick) {}

    farm::lang::Value call(
        const std::string& name,
        const std::vector<farm::lang::Value>& args) override;

    World& world() { return world_; }
    int drone() const { return drone_; }
    // # of tick-consuming actions this drone has performed (multi-drone
    // runner uses this to give each drone one action per round).
    uint64_t actions() const { return actions_; }

private:
    int arg_int(const std::vector<farm::lang::Value>& a, size_t i,
                const char* fn);
    void consume_tick();

    World& world_;
    uint64_t max_ticks_;
    Progression* prog_;  // optional: enables crop gating + unlock natives
    int drone_;
    bool auto_tick_;     // true: advance world per action (single-drone)
    uint64_t actions_ = 0;
};

}  // namespace farm::sim
