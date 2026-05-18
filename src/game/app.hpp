#pragma once
#include <memory>
#include <string>

#include "blueprint/codec.hpp"
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
    static constexpr int kW = 8;
    static constexpr int kH = 6;

    void rebuild();                 // (re)parse+compile source, reset world
    void resync_graph();            // text -> blueprint (no VM reset)
    void advance(int64_t budget);   // run VM, trap errors
    void step_one_action();         // run until one world tick elapses
    void draw_controls();
    void draw_editor();
    void draw_farm();
    void draw_blueprint();

    std::string source_;
    std::string status_;
    std::string error_;
    bool running_ = false;
    int speed_ = 300;  // VM instructions per frame when running

    std::unique_ptr<sim::World> world_;
    std::unique_ptr<sim::RobotHost> host_;
    std::unique_ptr<vm::Chunk> chunk_;
    std::unique_ptr<vm::VM> vm_;

    blueprint::Graph graph_;        // blueprint view of the current program
    blueprint::LayoutStore layout_; // node positions kept by AST id
    bool graph_laid_ = false;       // push codec layout once per (re)build
    bool src_dirty_ = false;        // text edited -> needs graph resync
};

}  // namespace farm::game
