#include "sim/world.hpp"

namespace farm::sim {

int crop_maturity(int crop) {
    switch (crop) {
        case CropWheat: return 5;     // 5 ticks to ripen
        case CropCarrot: return 8;    // slower, higher-tier crop
        case CropPumpkin: return 12;  // slowest, high yield
        default: return 0;
    }
}

int crop_base_yield(int crop) {
    switch (crop) {
        case CropWheat: return 1;
        case CropCarrot: return 1;
        case CropPumpkin: return 3;  // high yield
        default: return 0;
    }
}

int crop_companion(int crop) {
    switch (crop) {
        case CropWheat: return CropCarrot;  // wheat & carrot are companions
        case CropCarrot: return CropWheat;
        default: return CropNone;
    }
}

World::World(int width, int height)
    : w_(width), h_(height), grid_(static_cast<size_t>(width) * height) {}

int64_t World::inventory_of(int crop) const {
    auto it = inv_.find(crop);
    return it == inv_.end() ? 0 : it->second;
}

bool World::spend(int crop, int64_t n) {
    if (n < 0 || inventory_of(crop) < n) return false;
    inv_[crop] -= n;
    return true;
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
    if (crop != CropWheat && crop != CropCarrot && crop != CropPumpkin)
        return false;
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

bool World::fertilize() {
    Tile& t = at(rx_, ry_);
    if (t.crop == CropNone) return false;
    t.age = crop_maturity(t.crop);  // instantly ripe
    return true;
}

bool World::companion_adjacent(int x, int y, int crop) const {
    if (crop == CropNone) return false;
    const int dx[] = {0, 0, -1, 1};
    const int dy[] = {-1, 1, 0, 0};
    for (int i = 0; i < 4; ++i) {
        int nx = x + dx[i], ny = y + dy[i];
        if (in_bounds(nx, ny) && grid_[idx(nx, ny)].crop == crop)
            return true;
    }
    return false;
}

bool World::harvest() {
    Tile& t = at(rx_, ry_);
    if (t.crop == CropNone || t.age < crop_maturity(t.crop)) return false;
    int crop = t.crop;
    int64_t gain = crop_base_yield(crop);
    if (polyculture_ &&
        companion_adjacent(rx_, ry_, crop_companion(crop)))
        gain += 1;  // polyculture / companion-planting bonus
    inv_[crop] += gain;
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
    for (auto& t : grid_) {
        if (t.crop == CropNone) continue;
        int m = crop_maturity(t.crop);
        if (t.age >= m) continue;
        int growth = (watering_ && t.watered) ? 2 : 1;  // watering speeds 2x
        t.age = (t.age + growth > m) ? m : t.age + growth;
    }
    ++tick_;
}

}  // namespace farm::sim
