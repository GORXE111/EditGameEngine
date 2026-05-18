#include <doctest/doctest.h>

#include "lang/interpreter.hpp"

using namespace farm::lang;

namespace {

// Mock robot host: `emit(x)` logs an int, `add(a,b)` returns a+b,
// `ping()` counts calls. Stands in for the M2 sim API.
struct MockHost : Host {
    std::vector<int64_t> log;
    int pings = 0;

    Value call(const std::string& name,
               const std::vector<Value>& args) override {
        if (name == "emit") {
            if (args.size() != 1 || !args[0].is_int())
                throw RuntimeError("emit(int) expected");
            log.push_back(args[0].i);
            return Value::I(0);
        }
        if (name == "add") {
            if (args.size() != 2) throw RuntimeError("add arity");
            return Value::I(args[0].i + args[1].i);
        }
        if (name == "ping") {
            ++pings;
            return Value::I(pings);
        }
        throw RuntimeError("unknown native '" + name + "'");
    }
};

int64_t run_int(const std::string& src) {
    MockHost h;
    Value v = interpret(src, h);
    REQUIRE(v.is_int());
    return v.i;
}

}  // namespace

TEST_CASE("interp: arithmetic and precedence") {
    CHECK(run_int("return 1 + 2 * 3") == 7);
    CHECK(run_int("return (1 + 2) * 3") == 9);
    CHECK(run_int("return 17 % 5") == 2);
    CHECK(run_int("return 10 / 3") == 3);
    CHECK(run_int("return -(2 + 3)") == -5);
}

TEST_CASE("interp: booleans and short-circuit") {
    CHECK(run_int("if true and not false then return 1 end return 0") == 1);
    CHECK(run_int("if 3 < 2 or 2 == 2 then return 7 end return 0") == 7);
    // short-circuit: `or` must not evaluate rhs when lhs true
    MockHost h;
    CHECK(interpret("if true or ping() == 9 then return 0 end return 1", h)
              .i == 0);
    CHECK(h.pings == 0);
}

TEST_CASE("interp: while sum and repeat") {
    CHECK(run_int("s = 0\n"
                  "i = 1\n"
                  "while i <= 5 do s = s + i i = i + 1 end\n"
                  "return s") == 15);

    MockHost h;
    interpret("repeat 3 do emit(7) end", h);
    CHECK(h.log == std::vector<int64_t>{7, 7, 7});
}

TEST_CASE("interp: functions, recursion, forward reference") {
    const char* fib =
        "func fib(n)\n"
        "  if n < 2 then return n end\n"
        "  return fib(n - 1) + fib(n - 2)\n"
        "end\n"
        "return fib(10)";
    CHECK(run_int(fib) == 55);

    // forward reference: main() defined before helper()
    const char* fwd =
        "func main() return helper() + 1 end\n"
        "func helper() return 41 end\n"
        "return main()";
    CHECK(run_int(fwd) == 42);
}

TEST_CASE("interp: native calls and ordering") {
    MockHost h;
    Value r = interpret(
        "func twice(n) emit(n) emit(n) end\n"
        "twice(1)\n"
        "emit(add(2, 3))\n"
        "return 0",
        h);
    CHECK(r.i == 0);
    CHECK(h.log == std::vector<int64_t>{1, 1, 5});
}

TEST_CASE("interp: runtime errors") {
    MockHost h;
    CHECK_THROWS_AS(interpret("return x", h), RuntimeError);          // undef
    CHECK_THROWS_AS(interpret("return 1 / 0", h), RuntimeError);      // div0
    CHECK_THROWS_AS(interpret("return 1 + true", h), RuntimeError);   // type
    CHECK_THROWS_AS(interpret("if 1 then return 0 end", h),
                    RuntimeError);                                    // cond
    CHECK_THROWS_AS(interpret("nope()", h), RuntimeError);            // native
    CHECK_THROWS_AS(interpret("func f(a) return a end\nreturn f()", h),
                    RuntimeError);                                    // arity
}
