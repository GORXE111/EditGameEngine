#include "sim/world.hpp"

namespace farm::sim {

int crop_maturity(int crop) {
    switch (crop) {
        case CropWheat: return 5;  // 5 ticks to ripen
        default: return 0;
    }
}

World::World(int width, int height)
    : w_(width), h_(height), grid_(static_cast<size_t>(width) * height) {}

int64_t World::inventory_of(int crop) const {
    auto it = inv_.find(crop);
    return it == inv_.end() ? 0 : it->second;
}

bool World::move(int dir) {
    int nx = rx_, ny = ry_;
    switch (dir) {
        case North: --ny; break;
        case East:  ++nx; break;
        case South: ++ny; break;
        case West:  --nx; break;
        default: return false;
    }
    if (!in_bounds(nx, ny)) return false;  // blocked by edge, no move
    rx_ = nx;
    ry_ = ny;
    return true;
}

bool World::till() {
    Tile& t = at(rx_, ry_);
    if (t.tilled) return false;
    t.tilled = true;
    return true;
}

bool World::plant(int crop) {
    if (crop != CropWheat) return false;
    Tile& t = at(rx_, ry_);
    if (!t.tilled || t.crop != CropNone) return false;
    t.crop = crop;
    t.age = 0;
    t.watered = false;
    return true;
}

bool World::water() {
    Tile& t = at(rx_, ry_);
    if (t.crop == CropNone) return false;
    t.watered = true;
    return true;
}

bool World::harvest() {
    Tile& t = at(rx_, ry_);
    if (t.crop == CropNone || t.age < crop_maturity(t.crop)) return false;
    inv_[t.crop] += 1;
    t.crop = CropNone;
    t.age = 0;
    t.watered = false;
    t.tilled = false;  // soil must be re-tilled before replanting
    return true;
}

int World::sense() const {
    const Tile& t = grid_[idx(rx_, ry_)];
    if (t.crop != CropNone)
        return t.age >= crop_maturity(t.crop) ? SenseMature : SenseGrowing;
    return t.tilled ? SenseTilled : SenseWild;
}

bool World::can_harvest() const {
    const Tile& t = grid_[idx(rx_, ry_)];
    return t.crop != CropNone && t.age >= crop_maturity(t.crop);
}

void World::advance_tick() {
    for (auto& t : grid_)
        if (t.crop != CropNone && t.age < crop_maturity(t.crop)) ++t.age;
    ++tick_;
}

}  // namespace farm::sim
