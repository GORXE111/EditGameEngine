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
    if (world_.tick() >= max_ticks_)
        throw RuntimeError("tick budget exceeded (program never finishes?)");
    world_.advance_tick();
}

Value RobotHost::call(const std::string& name,
                      const std::vector<Value>& args) {
    // tick-consuming actions
    if (name == "move") {
        bool ok = world_.move(arg_int(args, 0, "move"));
        consume_tick();
        return Value::B(ok);
    }
    if (name == "till") {
        bool ok = world_.till();
        consume_tick();
        return Value::B(ok);
    }
    if (name == "plant") {
        bool ok = world_.plant(arg_int(args, 0, "plant"));
        consume_tick();
        return Value::B(ok);
    }
    if (name == "water") {
        bool ok = world_.water();
        consume_tick();
        return Value::B(ok);
    }
    if (name == "harvest") {
        bool ok = world_.harvest();
        consume_tick();
        return Value::B(ok);
    }
    if (name == "wait") {
        world_.wait();
        consume_tick();
        return Value::I(0);
    }
    // free queries
    if (name == "sense") return Value::I(world_.sense());
    if (name == "pos") return Value::I(world_.pos_index());
    if (name == "can_harvest") return Value::B(world_.can_harvest());
    if (name == "inventory")
        return Value::I(world_.inventory_of(arg_int(args, 0, "inventory")));

    throw RuntimeError("unknown robot action '" + name + "'");
}

}  // namespace farm::sim
