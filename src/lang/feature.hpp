#pragma once
#include <cstdint>

#include "lang/ast.hpp"

// Progression-gated language features (TFWR-style). The base language
// (assignment, return, operators, native calls) is always available; the
// control-flow constructs are unlocked over time. Higher layers derive a
// FeatureSet from the unlock tree and hand it to parse().
namespace farm::lang {

enum class Feature : uint32_t {
    Conditionals = 1u << 0,  // if / else
    Loops        = 1u << 1,  // while
    RepeatLoop   = 1u << 2,  // repeat N do
    Functions    = 1u << 3,  // func declarations + user calls
};

struct FeatureSet {
    uint32_t bits = 0;

    static FeatureSet all() {
        return FeatureSet{ (uint32_t)Feature::Conditionals |
                           (uint32_t)Feature::Loops |
                           (uint32_t)Feature::RepeatLoop |
                           (uint32_t)Feature::Functions };
    }
    static FeatureSet none() { return FeatureSet{0}; }

    bool has(Feature f) const { return (bits & (uint32_t)f) != 0; }
    FeatureSet& add(Feature f) { bits |= (uint32_t)f; return *this; }
};

const char* feature_name(Feature f);

}  // namespace farm::lang
