#pragma once
#include <cstdint>
#include <stdexcept>
#include <string>
#include <vector>

namespace farm::lang {

// FarmCode v1 has exactly two value types: 64-bit int and bool.
struct Value {
    enum class T { Int, Bool };
    T t = T::Int;
    int64_t i = 0;

    static Value I(int64_t v) { return Value{T::Int, v}; }
    static Value B(bool b) { return Value{T::Bool, b ? 1 : 0}; }

    bool is_int() const { return t == T::Int; }
    bool is_bool() const { return t == T::Bool; }
    bool boolean() const { return i != 0; }

    bool operator==(const Value& o) const { return t == o.t && i == o.i; }
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
