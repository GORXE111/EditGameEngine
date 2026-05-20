#include <doctest/doctest.h>

#include "blueprint/codec.hpp"
#include "lang/interpreter.hpp"
#include "lang/parser.hpp"
#include "lang/printer.hpp"
#include "sim/progress.hpp"
#include "vm/vm.hpp"

using namespace farm;

namespace {
struct NullHost : lang::Host {
    lang::Value call(const std::string& n,
                     const std::vector<lang::Value>&) override {
        throw lang::RuntimeError("no native '" + n + "'");
    }
};
int64_t ri(const std::string& src) {
    NullHost h;
    lang::Value v = lang::interpret(src, h);
    REQUIRE(v.is_int());
    return v.i;
}
std::string canon(const std::string& s) { return lang::print(lang::parse(s)); }
}  // namespace

TEST_CASE("lists: parser + printer round-trip is idempotent") {
    const char* progs[] = {
        "a = [1, 2, 3]\nreturn a[0]",
        "a = []\nappend(a, 7)\nreturn a[0]",
        "m = [[1, 2], [3, 4]]\nreturn m[1][0]",
        "a = [1, 2]\na[1] = a[0] + 9\nreturn a[1]",
        "a = [1, 2, 3]\nreturn len(a)",
    };
    for (auto* s : progs) {
        std::string c1 = canon(s);
        CHECK(lang::print(lang::parse(c1)) == c1);  // idempotent
    }
    CHECK(canon("a=[1,2,3]\nreturn a[0]") == "a = [1, 2, 3]\nreturn a[0]\n");
    CHECK(canon("a[1]=2") == "a[1] = 2\n");
}

TEST_CASE("lists: interpreter semantics") {
    CHECK(ri("a = [10, 20, 30]\nreturn a[1]") == 20);
    CHECK(ri("a = [1, 2, 3]\nreturn len(a)") == 3);
    CHECK(ri("a = []\nappend(a, 5)\nappend(a, 6)\nreturn a[0] + a[1]") == 11);
    CHECK(ri("a = [1, 2]\na[0] = 9\nreturn a[0]") == 9);
    CHECK(ri("m = [[1, 2], [3, 4]]\nreturn m[1][1]") == 4);
    // sum via while + indexing
    CHECK(ri("a = [4, 5, 6]\ni = 0\ns = 0\n"
             "while i < len(a) do s = s + a[i] i = i + 1 end\n"
             "return s") == 15);
}

TEST_CASE("lists: reference semantics (shared, mutable)") {
    // passing a list to a function and appending is visible to the caller
    CHECK(ri("func push2(x) append(x, 2) end\n"
             "a = [1]\npush2(a)\nreturn a[1]") == 2);
}

TEST_CASE("lists: runtime errors") {
    NullHost h;
    CHECK_THROWS_AS(lang::interpret("a = [1]\nreturn a[5]", h),
                    lang::RuntimeError);  // out of range
    CHECK_THROWS_AS(lang::interpret("return 3[0]", h),
                    lang::RuntimeError);  // not a list
    CHECK_THROWS_AS(lang::interpret("a = [1]\nreturn a[true]", h),
                    lang::RuntimeError);  // non-int index
    CHECK_THROWS_AS(lang::interpret("return len(1)", h),
                    lang::RuntimeError);  // len of non-list
}

TEST_CASE("lists: VM matches interpreter") {
    const char* progs[] = {
        "a = [3, 1, 4, 1, 5]\nreturn a[2] + a[4]",
        "a = []\nappend(a, 1)\nappend(a, 2)\nappend(a, 3)\nreturn len(a)",
        "a = [0, 0, 0]\ni = 0\nwhile i < 3 do a[i] = i * i i = i + 1 end\n"
        "return a[2]",
        "func make() return [9, 8, 7] end\nreturn make()[1]",
    };
    for (auto* s : progs) {
        lang::Program ast = lang::parse(s);
        NullHost hi, hv;
        lang::Value vi = lang::interpret(ast, hi);
        lang::Value vv = farm::vm::execute(ast, hv);
        CHECK(vi == vv);
    }
}

TEST_CASE("lists: gated behind the Lists unlock") {
    sim::Progression p;  // Lists locked
    lang::FeatureSet fs = p.feature_set();
    CHECK_THROWS_AS(lang::parse("a = [1, 2]", fs), lang::ParseError);
    CHECK_THROWS_AS(lang::parse("return a[0]", fs), lang::ParseError);
    CHECK_THROWS_AS(lang::parse("a[0] = 1", fs), lang::ParseError);
    CHECK_THROWS_AS(lang::parse("return len(x)", fs), lang::ParseError);
    CHECK_NOTHROW(lang::parse("x = 1\nreturn x", fs));  // base still ok

    p.grant(sim::U_Lists);
    CHECK_NOTHROW(lang::parse("a = [1, 2]\nreturn a[0]", p.feature_set()));
}

TEST_CASE("lists: blueprint codec round-trips list programs") {
    const char* progs[] = {
        "a = [1, 2, 3]\nb = a[1]\nreturn b",
        "a = []\nappend(a, 5)\na[0] = a[0] + 1\nreturn a[0]",
        "m = [[1], [2]]\nreturn m[0][0] + m[1][0]",
    };
    for (auto* s : progs) {
        lang::Program a1 = lang::parse(s);
        lang::Program a2 = blueprint::to_ast(blueprint::build(a1));
        CHECK(lang::print(a1) == lang::print(a2));
    }
}
