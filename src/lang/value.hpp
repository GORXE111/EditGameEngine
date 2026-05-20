#pragma once
#include <cstdint>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

namespace farm::lang {

// FarmCode values: 64-bit int, bool, and (v2) list. Lists are reference
// types (shared, mutable) like Lua tables / Python lists.
struct Value {
    enum class T { Int, Bool, List };
    T t = T::Int;
    int64_t i = 0;
    std::shared_ptr<std::vector<Value>> list;  // valid when t == List

    static Value I(int64_t v) { return Value{T::Int, v}; }
    static Value B(bool b) { return Value{T::Bool, b ? 1 : 0}; }
    static Value L(std::shared_ptr<std::vector<Value>> l) {
        Value v;
        v.t = T::List;
        v.list = std::move(l);
        return v;
    }
    static Value L() {
        return L(std::make_shared<std::vector<Value>>());
    }

    bool is_int() const { return t == T::Int; }
    bool is_bool() const { return t == T::Bool; }
    bool is_list() const { return t == T::List; }
    bool boolean() const { return i != 0; }

    bool operator==(const Value& o) const {
        if (t != o.t) return false;
        if (t == T::List) return list.get() == o.list.get();  // identity
        return i == o.i;
    }
};

struct RuntimeError : std::runtime_error {
    explicit RuntimeError(const std::string& m) : std::runtime_error(m) {}
};

// Host-provided native functions (the robot API in M2; mock in M1 tests).
// A native may have side effects on the host world. The resumable VM (M1.4)
// adds suspension on top; the tree interpreter calls natives synchronously.
struct Host {
    virtual ~Host() = default;
    // Throw RuntimeError for unknown names / bad arity.
    virtual Value call(const std::string& name,
                       const std::vector<Value>& args) = 0;
};

}  // namespace farm::lang
