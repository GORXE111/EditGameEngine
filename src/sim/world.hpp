#pragma once
#include <cstdint>
#include <unordered_map>
#include <utility>
#include <vector>

// Farm world: a W x H grid worked by one or more drones. Engine-independent
// (no SDL) so it stays headless-testable. Time advances in discrete ticks;
// crops grow as ticks pass. The robot API in robot_host.hpp drives this.
//
// Single-drone methods operate on drone 0 (back-compatible); the indexed
// overloads target a specific drone for the multi-drone runner.
namespace farm::sim {

enum Dir : int { North = 0, East = 1, South = 2, West = 3 };
enum CropId : int {
    CropNone = 0, CropWheat = 1, CropCarrot = 2, CropPumpkin = 3
};

// sense() result codes.
enum Sensed : int { SenseWild = 0, SenseTilled = 1, SenseGrowing = 2,
                    SenseMature = 3 };

struct Tile {
    bool tilled = false;
    int crop = CropNone;  // CropId
    int age = 0;          // ticks since planted
    bool watered = false; // reserved for later mechanics
};

int crop_maturity(int crop);     // ticks to ripen; 0 for CropNone
int crop_base_yield(int crop);   // units gained on harvest
int crop_companion(int crop);    // companion crop id (CropNone if none)

class World {
public:
    World(int width, int height);

    int width() const { return w_; }
    int height() const { return h_; }
    uint64_t tick() const { return tick_; }

    // ---- drones ----
    int drones() const { return static_cast<int>(drones_.size()); }
    int add_drone();                  // spawns at origin; returns its id
    int drone_x(int d) const { return drones_[d].first; }
    int drone_y(int d) const { return drones_[d].second; }
    int rx() const { return drones_[0].first; }   // back-compat: drone 0
    int ry() const { return drones_[0].second; }

    const Tile& tile(int x, int y) const { return grid_[idx(x, y)]; }
    int64_t inventory_of(int crop) const;
    bool spend(int crop, int64_t n);  // false (no change) if short

    bool in_bounds(int x, int y) const {
        return x >= 0 && y >= 0 && x < w_ && y < h_;
    }

    // ---- tick-consuming actions (per drone; return whether it did work) ----
    bool move(int drone, int dir);
    bool till(int drone);
    bool plant(int drone, int crop);
    bool water(int drone);
    bool fertilize(int drone);
    bool harvest(int drone);

    // single-drone shortcuts (drone 0)
    bool move(int dir) { return move(0, dir); }
    bool till() { return till(0); }
    bool plant(int crop) { return plant(0, crop); }
    bool water() { return water(0); }
    bool fertilize() { return fertilize(0); }
    bool harvest() { return harvest(0); }
    void wait() {}  // pure time pass

    // Rule toggles (set by the host from the unlock tree).
    void set_watering(bool on) { watering_ = on; }
    void set_polyculture(bool on) { polyculture_ = on; }

    // ---- queries (no tick) ----
    int sense(int drone) const;
    int pos_index(int drone) const {
        return drones_[drone].second * w_ + drones_[drone].first;
    }
    bool can_harvest(int drone) const;
    int sense() const { return sense(0); }
    int pos_index() const { return pos_index(0); }
    bool can_harvest() const { return can_harvest(0); }

    void advance_tick();  // grow crops one tick

private:
    int idx(int x, int y) const { return y * w_ + x; }
    Tile& at(int x, int y) { return grid_[idx(x, y)]; }
    bool companion_adjacent(int x, int y, int crop) const;

    int w_, h_;
    uint64_t tick_ = 0;
    bool watering_ = false;     // watered tiles grow 2x
    bool polyculture_ = false;  // +1 yield next to companion crop
    std::vector<std::pair<int, int>> drones_;  // [0] always exists
    std::vector<Tile> grid_;
    std::unordered_map<int, int64_t> inv_;
};

}  // namespace farm::sim
