#pragma once
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

#include "lang/ast.hpp"
#include "lang/value.hpp"
#include "vm/bytecode.hpp"

namespace farm::vm {

std::string version();  // smoke test

// Resumable stack VM. Execution is externally driven via run(budget):
// it advances at most `budget` instructions (negative = unlimited) and
// reports Paused if the budget runs out mid-program. The full VM state
// lives in this object (no host C++ recursion for FarmCode calls), so it
// can be paused, single-stepped, and later serialized — the design
// borrowed from UE5 FFrame (see .claude/rules/references.md).
class VM {
public:
    enum class Status { Completed, Paused };
    struct RunResult {
        Status status;
        farm::lang::Value value;  // valid when Completed
        uint64_t steps;           // instructions executed this call
    };

    VM(const Chunk& chunk, farm::lang::Host& host);

    RunResult run(int64_t budget = -1);
    bool finished() const { return finished_; }

private:
    struct Frame {
        const Function* fn;
        size_t ip = 0;
        std::unordered_map<std::string, farm::lang::Value> locals;
        bool is_global = false;
    };

    const Chunk& c_;
    farm::lang::Host& host_;
    std::vector<farm::lang::Value> stack_;
    std::vector<Frame> frames_;
    std::unordered_map<std::string, farm::lang::Value> globals_;
    bool finished_ = false;
    farm::lang::Value result_{};

    farm::lang::Value pop();
    void push(farm::lang::Value v) { stack_.push_back(v); }
    farm::lang::Value* resolve(Frame& f, const std::string& name);
    void do_store(Frame& f, const std::string& name, farm::lang::Value v);
    void exec_binary(Op op);
};

// Convenience: compile + run to completion, returning the program result.
farm::lang::Value execute(const farm::lang::Program& program,
                          farm::lang::Host& host);
farm::lang::Value execute(const std::string& source,
                          farm::lang::Host& host);

}  // namespace farm::vm
