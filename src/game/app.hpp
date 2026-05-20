#pragma once
#include <memory>
#include <string>
#include <vector>

#include "blueprint/codec.hpp"
#include "sim/progress.hpp"
#include "sim/robot_host.hpp"
#include "sim/world.hpp"
#include "vm/bytecode.hpp"
#include "vm/vm.hpp"

namespace farm::game {

// Owns the live program + world + VM and renders the M3 playable shell:
// a farm view, run/pause/step/speed controls, and a code editor. The VM is
// driven cooperatively each frame so an infinite `while true` never freezes
// the window (resumability from M1).
class App {
public:
    App();
    void frame();  // call once per rendered frame (UI + simulation step)

private:
    void rebuild();                 // (re)parse+compile source, reset world
    void resync_graph();            // text -> blueprint (no VM reset)
    bool all_finished() const;      // every drone VM done (or no program)
    bool do_round();                // each live drone one action + tick
    void advance(int64_t budget);   // run the fleet, trap errors
    void step_one_action();         // one round
    void draw_controls();
    void draw_editor();
    void draw_farm();
    void draw_blueprint();
    void draw_tech();               // unlock tree (progression)
    void draw_recipes();            // player-facing recipe library
    void insert_recipe(const char* snippet);  // append + rebuild
    void handle_graph_edits();      // wires/spawn/delete after EndNodeEditor
    void draw_palette_popup();      // right-click palette to spawn nodes

    std::string source_;
    std::string status_;
    std::string error_;
    bool running_ = false;
    int speed_ = 300;  // VM instructions per frame when running

    sim::Progression prog_;  // persists across Reset (permanent progression)
    std::unique_ptr<sim::World> world_;
    std::unique_ptr<vm::Chunk> chunk_;
    // One VM per drone over the same bytecode (N=1 == single-drone).
    std::vector<std::unique_ptr<sim::RobotHost>> hosts_;
    std::vector<std::unique_ptr<vm::VM>> vms_;

    blueprint::Graph graph_;        // blueprint view of the current program
    blueprint::LayoutStore layout_; // node positions kept by AST id
    bool graph_laid_ = false;       // push codec layout once per (re)build
    bool src_dirty_ = false;        // text edited -> needs graph resync

    // ---- E4 structural editing state ----
    // Per-frame translation from imnodes link-id to graph edge it represents.
    // Indexed by link_id (the int we pass to ImNodes::Link).
    struct EdgeRef {
        enum Kind { Exec, Then, Els, ValIn } kind;
        int owner;   // node index that owns the edge slot
        int slot;    // ValIn: index into Node.in[]; ignored otherwise
    };
    std::vector<EdgeRef> link_table_;
    bool open_palette_ = false;
    float palette_screen_x_ = 0, palette_screen_y_ = 0;
};

}  // namespace farm::game
