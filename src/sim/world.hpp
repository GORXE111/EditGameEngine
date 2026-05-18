#pragma once
#include <cstdint>
#include <unordered_map>
#include <vector>

// Farm world: a W x H grid the robot works. Engine-independent (no SDL) so
// it stays headless-testable. Time advances in discrete ticks; crops grow
// as ticks pass. The robot API in robot_host.hpp drives this.
namespace farm::sim {

enum Dir : int { North = 0, East = 1, South = 2, West = 3 };
enum CropId : int { CropNone = 0, CropWheat = 1, CropCarrot = 2 };

// sense() result codes.
enum Sensed : int { SenseWild = 0, SenseTilled = 1, SenseGrowing = 2,
                    SenseMature = 3 };

struct Tile {
    bool tilled = false;
    int crop = CropNone;  // CropId
    int age = 0;          // ticks since planted
    bool watered = false; // reserved for later mechanics
};

int crop_maturity(int crop);  // ticks to ripen; 0 for CropNone

class World {
public:
    World(int width, int height);

    int width() const { return w_; }
    int height() const { return h_; }
    int rx() const { return rx_; }
    int ry() const { return ry_; }
    uint64_t tick() const { return tick_; }

    const Tile& tile(int x, int y) const { return grid_[idx(x, y)]; }
    const Tile& here() const { return grid_[idx(rx_, ry_)]; }
    int64_t inventory_of(int crop) const;
    // Deduct n of a crop from inventory; false (and no change) if short.
    bool spend(int crop, int64_t n);

    bool in_bounds(int x, int y) const {
        return x >= 0 && y >= 0 && x < w_ && y < h_;
    }

    // ---- tick-consuming actions (return whether the action did something)
    bool move(int dir);   // false if blocked by edge
    bool till();          // false if already tilled
    bool plant(int crop); // false if not tilled / occupied / bad id
    bool water();
    bool harvest();       // false if nothing mature here
    void wait() {}        // pure time pass

    // ---- queries (no tick) ----
    int sense() const;
    int pos_index() const { return ry_ * w_ + rx_; }
    bool can_harvest() const;

    // Advance world by one tick (grow crops). Called by the host after each
    // tick-consuming action.
    void advance_tick();

private:
    int idx(int x, int y) const { return y * w_ + x; }
    Tile& at(int x, int y) { return grid_[idx(x, y)]; }

    int w_, h_;
    int rx_ = 0, ry_ = 0;
    uint64_t tick_ = 0;
    std::vector<Tile> grid_;
    std::unordered_map<int, int64_t> inv_;
};

}  // namespace farm::sim
