#include "sim/progress.hpp"

namespace farm::sim {

const UnlockDef& Progression::def(int id) {
    static const std::array<UnlockDef, U_COUNT> table = {{
        {"Conditionals", 3},
        {"Loops", 5},
        {"Repeat", 4},
        {"Functions", 8},
        {"Carrot", 6},
        {"Expand", 10},
        {"Watering", 8},
        {"Fertilizer", 14},
        {"Pumpkin", 12},
        {"Polyculture", 16},
        {"Drones", 20},
        {"Lists", 18},
    }};
    static const UnlockDef unknown{"?", 0};
    if (id < 0 || id >= U_COUNT) return unknown;
    return table[static_cast<size_t>(id)];
}

farm::lang::FeatureSet Progression::feature_set() const {
    using farm::lang::Feature;
    farm::lang::FeatureSet fs = farm::lang::FeatureSet::none();
    if (is_unlocked(U_Conditionals)) fs.add(Feature::Conditionals);
    if (is_unlocked(U_Loops)) fs.add(Feature::Loops);
    if (is_unlocked(U_Repeat)) fs.add(Feature::RepeatLoop);
    if (is_unlocked(U_Functions)) fs.add(Feature::Functions);
    if (is_unlocked(U_Lists)) fs.add(Feature::Lists);
    return fs;
}

}  // namespace farm::sim
