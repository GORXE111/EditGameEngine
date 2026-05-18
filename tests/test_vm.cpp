#include <doctest/doctest.h>

#include "lang/interpreter.hpp"
#include "lang/parser.hpp"
#include "vm/compiler.hpp"
#include "vm/vm.hpp"

using namespace farm::lang;

namespace {

struct MockHost : Host {
    std::vector<int64_t> log;
    int pings = 0;
    Value call(const std::string& name,
               const std::vector<Value>& args) override {
        if (name == "emit") { log.push_back(args.at(0).i); return Value::I(0); }
        if (name == "add") return Value::I(args.at(0).i + args.at(1).i);
        if (name == "ping") return Value::I(++pings);
        throw RuntimeError("unknown native '" + name + "'");
    }
};

}  // namespace

// The VM must produce identical results AND identical host side effects as
// the reference tree interpreter, for every golden program.
TEST_CASE("vm<->interp parity") {
    const char* progs[] = {
        "return 1 + 2 * 3",
        "return (1 + 2) * 3 % 4 - -5",
        "if true and not false then return 1 end return 0",
        "s=0 i=1 while i<=100 do s=s+i i=i+1 end return s",
        "n=0 repeat 7 do n=n+2 end return n",
        "func fib(n) if n<2 then return n end return fib(n-1)+fib(n-2) end\n"
        "return fib(15)",
        "func twice(n) emit(n) emit(n) end twice(3) emit(add(4,5)) return 0",
    };
    for (auto* s : progs) {
        Program ast = parse(s);
        MockHost hi, hv;
        Value vi = interpret(ast, hi);
        Value vv = farm::vm::execute(ast, hv);
        CHECK(vi == vv);
        CHECK(hi.log == hv.log);
    }
}

TEST_CASE("vm: short-circuit matches interpreter (no ping)") {
    MockHost h;
    Value v = farm::vm::execute(
        "if true or ping() == 9 then return 0 end return 1", h);
    CHECK(v.i == 0);
    CHECK(h.pings == 0);
}

TEST_CASE("vm: resumable — infinite loop never blocks") {
    MockHost h;
    farm::vm::Chunk ch = farm::vm::compile(parse("n=0 while true do n=n+1 end"));
    farm::vm::VM vm(ch, h);

    auto r1 = vm.run(500);
    CHECK(r1.status == farm::vm::VM::Status::Paused);
    CHECK(r1.steps == 500);
    CHECK_FALSE(vm.finished());

    auto r2 = vm.run(500);  // still going, state intact
    CHECK(r2.status == farm::vm::VM::Status::Paused);
    CHECK_FALSE(vm.finished());
}

TEST_CASE("vm: cooperative completion across many small budgets") {
    MockHost h;
    farm::vm::Chunk ch = farm::vm::compile(
        parse("s=0 i=1 while i<=1000 do s=s+i i=i+1 end return s"));
    farm::vm::VM vm(ch, h);

    farm::vm::VM::RunResult r{farm::vm::VM::Status::Paused, {}, 0};
    int rounds = 0;
    do {
        r = vm.run(50);  // tiny budget -> many pauses
        ++rounds;
    } while (r.status == farm::vm::VM::Status::Paused && rounds < 100000);

    CHECK(r.status == farm::vm::VM::Status::Completed);
    CHECK(r.value == Value::I(500500));
    CHECK(rounds > 1);  // proves it actually paused & resumed
}

TEST_CASE("vm: single-step (budget=1) reaches correct result") {
    MockHost h;
    farm::vm::Chunk ch = farm::vm::compile(parse("return 2 + 3 * 4"));
    farm::vm::VM vm(ch, h);
    int steps = 0;
    while (!vm.finished() && steps < 1000) {
        vm.run(1);
        ++steps;
    }
    REQUIRE(vm.finished());
    CHECK(vm.run().value == Value::I(14));  // re-query: completed
}

TEST_CASE("vm: runtime errors mirror interpreter") {
    MockHost h;
    CHECK_THROWS_AS(farm::vm::execute("return x", h), RuntimeError);
    CHECK_THROWS_AS(farm::vm::execute("return 1 / 0", h), RuntimeError);
    CHECK_THROWS_AS(farm::vm::execute("return 1 + true", h), RuntimeError);
    CHECK_THROWS_AS(farm::vm::execute("if 1 then return 0 end", h),
                    RuntimeError);
    CHECK_THROWS_AS(farm::vm::execute("nope()", h), RuntimeError);
    CHECK_THROWS_AS(
        farm::vm::execute("func f(a) return a end return f()", h),
        RuntimeError);
}
