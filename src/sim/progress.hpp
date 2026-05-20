#pragma once
#include <array>
#include <string>
#include <unordered_set>

#include "lang/feature.hpp"

// TFWR-style continuous progression: harvested crops are currency, spent in
// an unlock tree that grants crops, a bigger farm, more native API, and
// language features. No discrete levels. Engine-independent.
namespace farm::sim {

enum Unlock : int {
    U_Conditionals = 0,  // language: if/else
    U_Loops,             // language: while
    U_Repeat,            // language: repeat
    U_Functions,         // language: func
    U_Carrot,            // content: carrot crop
    U_Expand,            // world: larger farm
    U_Watering,          // mechanic: watering speeds growth 2x
    U_Fertilizer,        // mechanic: fertilize() instant-ripens
    U_Pumpkin,           // content: pumpkin crop (high yield)
    U_Polyculture,       // mechanic: companion-planting yield bonus
    U_Drones,            // multi-drone (shared program)
    U_Lists,             // language: arrays/lists ([], indexing, len/append)
    U_COUNT
};

struct UnlockDef {
    const char* name;
    int cost_wheat;  // v1: single-resource costs (wheat)
};

class Progression {
public:
    static const UnlockDef& def(int id);

    bool is_unlocked(int id) const { return done_.count(id) != 0; }
    int cost_of(int id) const { return def(id).cost_wheat; }

    // Marks an unlock done (caller is responsible for paying its cost).
    void grant(int id) { if (id >= 0 && id < U_COUNT) done_.insert(id); }

    // Language features currently available (base = none of the gated ones).
    farm::lang::FeatureSet feature_set() const;

    bool crop_carrot_unlocked() const { return is_unlocked(U_Carrot); }
    bool crop_pumpkin_unlocked() const { return is_unlocked(U_Pumpkin); }
    bool watering_unlocked() const { return is_unlocked(U_Watering); }
    bool fertilizer_unlocked() const { return is_unlocked(U_Fertilizer); }
    bool polyculture_unlocked() const { return is_unlocked(U_Polyculture); }
    int max_farm_size() const { return is_unlocked(U_Expand) ? 6 : 4; }
    int drone_count() const { return is_unlocked(U_Drones) ? 2 : 1; }

private:
    std::unordered_set<int> done_;
};

}  // namespace farm::sim
