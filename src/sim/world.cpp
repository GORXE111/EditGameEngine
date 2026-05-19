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
    : w_(width), h_(height),
      drones_(1, {0, 0}),
      grid_(static_cast<size_t>(width) * height) {}

int World::add_drone() {
    drones_.push_back({0, 0});
    return static_cast<int>(drones_.size()) - 1;
}

int64_t World::inventory_of(int crop) const {
    auto it = inv_.find(crop);
    return it == inv_.end() ? 0 : it->second;
}

bool World::spend(int crop, int64_t n) {
    if (n < 0 || inventory_of(crop) < n) return false;
    inv_[crop] -= n;
    return true;
}

bool World::move(int drone, int dir) {
    int nx = drones_[drone].first, ny = drones_[drone].second;
    switch (dir) {
        case North: --ny; break;
        case East:  ++nx; break;
        case South: ++ny; break;
        case West:  --nx; break;
        default: return false;
    }
    if (!in_bounds(nx, ny)) return false;  // blocked by edge, no move
    drones_[drone] = {nx, ny};
    return true;
}

bool World::till(int drone) {
    Tile& t = at(drones_[drone].first, drones_[drone].second);
    if (t.tilled) return false;
    t.tilled = true;
    return true;
}

bool World::plant(int drone, int crop) {
    if (crop != CropWheat && crop != CropCarrot && crop != CropPumpkin)
        return false;
    Tile& t = at(drones_[drone].first, drones_[drone].second);
    if (!t.tilled || t.crop != CropNone) return false;
    t.crop = crop;
    t.age = 0;
    t.watered = false;
    return true;
}

bool World::water(int drone) {
    Tile& t = at(drones_[drone].first, drones_[drone].second);
    if (t.crop == CropNone) return false;
    t.watered = true;
    return true;
}

bool World::fertilize(int drone) {
    Tile& t = at(drones_[drone].first, drones_[drone].second);
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

bool World::harvest(int drone) {
    int x = drones_[drone].first, y = drones_[drone].second;
    Tile& t = at(x, y);
    if (t.crop == CropNone || t.age < crop_maturity(t.crop)) return false;
    int crop = t.crop;
    int64_t gain = crop_base_yield(crop);
    if (polyculture_ && companion_adjacent(x, y, crop_companion(crop)))
        gain += 1;  // polyculture / companion-planting bonus
    inv_[crop] += gain;
    t.crop = CropNone;
    t.age = 0;
    t.watered = false;
    t.tilled = false;  // soil must be re-tilled before replanting
    return true;
}

int World::sense(int drone) const {
    const Tile& t = grid_[idx(drones_[drone].first, drones_[drone].second)];
    if (t.crop != CropNone)
        return t.age >= crop_maturity(t.crop) ? SenseMature : SenseGrowing;
    return t.tilled ? SenseTilled : SenseWild;
}

bool World::can_harvest(int drone) const {
    const Tile& t = grid_[idx(drones_[drone].first, drones_[drone].second)];
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
