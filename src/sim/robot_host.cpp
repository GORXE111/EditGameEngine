#include "sim/robot_host.hpp"

namespace farm::sim {
using farm::lang::RuntimeError;
using farm::lang::Value;

int RobotHost::arg_int(const std::vector<Value>& a, size_t i,
                       const char* fn) {
    if (i >= a.size() || !a[i].is_int())
        throw RuntimeError(std::string(fn) + ": expected int argument");
    return static_cast<int>(a[i].i);
}

void RobotHost::consume_tick() {
    ++actions_;
    if (auto_tick_) {  // single-drone: world advances per action
        if (world_.tick() >= max_ticks_)
            throw RuntimeError(
                "tick budget exceeded (program never finishes?)");
        world_.advance_tick();
    }
    // manual mode: the multi-drone runner advances the world per round.
}

Value RobotHost::call(const std::string& name,
                      const std::vector<Value>& args) {
    // Keep world growth rules in sync with the unlock tree so Tech-panel
    // unlocks take effect immediately.
    if (prog_) {
        world_.set_watering(prog_->watering_unlocked());
        world_.set_polyculture(prog_->polyculture_unlocked());
    }
    const int d = drone_;

    // tick-consuming actions
    if (name == "move") {
        bool ok = world_.move(d, arg_int(args, 0, "move"));
        consume_tick();
        return Value::B(ok);
    }
    if (name == "till") {
        bool ok = world_.till(d);
        consume_tick();
        return Value::B(ok);
    }
    if (name == "plant") {
        int crop = arg_int(args, 0, "plant");
        bool locked = prog_ &&
            ((crop == CropCarrot && !prog_->crop_carrot_unlocked()) ||
             (crop == CropPumpkin && !prog_->crop_pumpkin_unlocked()));
        bool ok = !locked && world_.plant(d, crop);
        consume_tick();
        return Value::B(ok);
    }
    if (name == "fertilize") {
        bool locked = prog_ && !prog_->fertilizer_unlocked();
        bool ok = !locked && world_.fertilize(d);
        consume_tick();
        return Value::B(ok);
    }
    if (name == "water") {
        bool ok = world_.water(d);
        consume_tick();
        return Value::B(ok);
    }
    if (name == "harvest") {
        bool ok = world_.harvest(d);
        consume_tick();
        return Value::B(ok);
    }
    if (name == "wait") {
        consume_tick();
        return Value::I(0);
    }

    // free queries
    if (name == "sense") return Value::I(world_.sense(d));
    if (name == "pos") return Value::I(world_.pos_index(d));
    if (name == "can_harvest") return Value::B(world_.can_harvest(d));
    if (name == "get_drone_id") return Value::I(d);
    if (name == "num_drones") return Value::I(world_.drones());
    if (name == "inventory")
        return Value::I(world_.inventory_of(arg_int(args, 0, "inventory")));
    if (name == "num_items")
        return Value::I(world_.inventory_of(arg_int(args, 0, "num_items")));

    // progression / economy natives (only when a Progression is attached)
    if (prog_) {
        if (name == "unlock") {
            int id = arg_int(args, 0, "unlock");
            int cost = prog_->cost_of(id);
            bool ok = !prog_->is_unlocked(id) &&
                      world_.spend(CropWheat, cost);
            if (ok) prog_->grant(id);
            consume_tick();
            return Value::B(ok);
        }
        if (name == "get_cost")
            return Value::I(prog_->cost_of(arg_int(args, 0, "get_cost")));
        if (name == "num_unlocked")
            return Value::I(
                prog_->is_unlocked(arg_int(args, 0, "num_unlocked")) ? 1
                                                                     : 0);
    }

    throw RuntimeError("unknown robot action '" + name + "'");
}

}  // namespace farm::sim
