#pragma once
#include <string>
#include <vector>

#include "sim/progress.hpp"

// Player-facing "blueprint recipes": named, curated code snippets that
// solve a common task (plant a tile, sweep a row, harvest when ripe...).
// Inserting a recipe appends its snippet to the program; gated recipes
// are shown but disabled until the required tech-tree unlocks are met.
namespace farm::game {

struct Recipe {
    const char* id;            // stable; used for i18n keys
    const char* snippet;       // text appended to the program on Insert
    std::vector<int> needs;    // sim::Unlock ids that must be granted
};

const std::vector<Recipe>& recipes();

// All required unlocks granted.
bool recipe_available(const Recipe& r, const sim::Progression& p);

}  // namespace farm::game
