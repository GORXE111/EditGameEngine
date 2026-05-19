#include "game/app.hpp"

#include <exception>

#include "imgui.h"
#include "imgui_stdlib.h"
#include "imnodes.h"
#include "blueprint/codec.hpp"
#include "lang/parser.hpp"
#include "lang/printer.hpp"
#include "vm/compiler.hpp"

namespace farm::game {

namespace {
// Start of progression: loops / conditionals / functions are LOCKED.
// Base language only (assignment, native calls, return). Farm wheat,
// then open the Tech panel and unlock features to scale up.
const char* kExample =
    "# 起步：循环/条件/函数还没解锁。\n"
    "# 用直线代码种几格小麦攒资源，再去 Tech 面板解锁。\n"
    "till() plant(1)\n"
    "wait() wait() wait() wait() wait()\n"
    "harvest()\n"
    "move(1)\n"
    "till() plant(1)\n"
    "wait() wait() wait() wait() wait()\n"
    "harvest()\n"
    "move(1)\n"
    "till() plant(1)\n"
    "wait() wait() wait() wait() wait()\n"
    "harvest()\n"
    "return num_items(1)\n";
}  // namespace

App::App() {
    source_ = kExample;
    rebuild();
}

void App::rebuild() {
    error_.clear();
    running_ = false;
    hosts_.clear();
    vms_.clear();
    try {
        lang::Program prog = lang::parse(source_, prog_.feature_set());
        graph_ = blueprint::build(prog, layout_);  // keep node positions
        graph_laid_ = false;
        chunk_ = std::make_unique<vm::Chunk>(vm::compile(prog));
        int n = prog_.max_farm_size();
        world_ = std::make_unique<sim::World>(n, n);

        int ndrones = prog_.drone_count();
        while (world_->drones() < ndrones) world_->add_drone();
        for (int i = 0; i < ndrones; ++i) {
            hosts_.push_back(std::make_unique<sim::RobotHost>(
                *world_, 1'000'000'000ull, &prog_, i, /*auto_tick*/ false));
            vms_.push_back(std::make_unique<vm::VM>(*chunk_, *hosts_[i]));
        }
    } catch (const std::exception& e) {
        error_ = e.what();
    }
}

bool App::all_finished() const {
    if (vms_.empty()) return true;
    for (auto& v : vms_)
        if (!v->finished()) return false;
    return true;
}

// One round: every live drone single-steps until it performs one timed
// action (or its program ends); then the world advances one shared tick.
bool App::do_round() {
    bool any_live = false;
    for (size_t i = 0; i < vms_.size(); ++i) {
        if (vms_[i]->finished()) continue;
        any_live = true;
        uint64_t a0 = hosts_[i]->actions();
        for (int guard = 0;
             !vms_[i]->finished() && hosts_[i]->actions() == a0 &&
                 guard < 500000;
             ++guard)
            vms_[i]->run(1);
    }
    if (any_live && world_) world_->advance_tick();
    return any_live;
}

// Text edit -> re-derive the blueprint, preserving node positions by AST id.
// Does not touch the running VM/world (only Apply/Reset does).
void App::resync_graph() {
    try {
        lang::Program prog = lang::parse(source_, prog_.feature_set());
        graph_ = blueprint::build(prog, layout_);
        graph_laid_ = false;
        error_.clear();
    } catch (const std::exception& e) {
        error_ = e.what();  // keep last good graph_
    }
}

void App::advance(int64_t budget) {
    if (all_finished()) return;
    int rounds = static_cast<int>(budget / 16);
    if (rounds < 1) rounds = 1;
    try {
        for (int r = 0; r < rounds; ++r)
            if (!do_round()) break;
    } catch (const std::exception& e) {
        error_ = e.what();
        running_ = false;
    }
}

void App::step_one_action() {
    if (all_finished()) return;
    try {
        do_round();
    } catch (const std::exception& e) {
        error_ = e.what();
        running_ = false;
    }
}

void App::frame() {
    if (src_dirty_) { resync_graph(); src_dirty_ = false; }
    if (running_) advance(speed_);
    draw_controls();
    draw_editor();
    draw_farm();
    draw_blueprint();
    draw_tech();
}

void App::draw_controls() {
    ImGui::SetNextWindowPos({10, 10}, ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize({360, 230}, ImGuiCond_FirstUseEver);
    ImGui::Begin("Control");

    const bool has_prog = !vms_.empty();
    const bool done = all_finished();
    if (ImGui::Button(running_ ? "Pause" : "Run") && has_prog)
        running_ = !running_;
    ImGui::SameLine();
    if (ImGui::Button("Step")) { running_ = false; step_one_action(); }
    ImGui::SameLine();
    if (ImGui::Button("Reset")) rebuild();

    ImGui::SliderInt("Speed", &speed_, 1, 4000, "%d instr/frame");

    const char* st = !error_.empty() ? "ERROR"
                     : !has_prog     ? "no program"
                     : done          ? "completed"
                     : running_      ? "running"
                                     : "paused";
    ImGui::Text("Status: %s", st);
    if (world_) {
        ImGui::Text("Tick: %llu  |  Drones: %d",
                    static_cast<unsigned long long>(world_->tick()),
                    world_->drones());
        for (int i = 0; i < world_->drones(); ++i)
            ImGui::Text("  drone %d: (%d, %d)", i, world_->drone_x(i),
                        world_->drone_y(i));
        ImGui::Text("Wheat %lld  Carrot %lld  Pumpkin %lld",
                    (long long)world_->inventory_of(sim::CropWheat),
                    (long long)world_->inventory_of(sim::CropCarrot),
                    (long long)world_->inventory_of(sim::CropPumpkin));
    }
    if (!error_.empty())
        ImGui::TextWrapped("%s", error_.c_str());
    ImGui::End();
}

void App::draw_editor() {
    ImGui::SetNextWindowPos({10, 250}, ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize({360, 420}, ImGuiCond_FirstUseEver);
    ImGui::Begin("Code");
    if (ImGui::Button("Apply & Reset")) rebuild();
    ImGui::SameLine();
    ImGui::TextDisabled("(edit, then Apply)");
    ImVec2 sz = ImGui::GetContentRegionAvail();
    if (ImGui::InputTextMultiline("##src", &source_, sz))
        src_dirty_ = true;  // live text -> blueprint sync next frame
    ImGui::End();
}

void App::draw_farm() {
    ImGui::SetNextWindowPos({380, 10}, ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize({660, 540}, ImGuiCond_FirstUseEver);
    ImGui::Begin("Farm");
    if (!world_) { ImGui::TextUnformatted("no world"); ImGui::End(); return; }

    const int gw = world_->width(), gh = world_->height();
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 org = ImGui::GetCursorScreenPos();
    ImVec2 avail = ImGui::GetContentRegionAvail();
    float cell = avail.x / gw;
    float maxcy = avail.y / gh;
    if (maxcy < cell) cell = maxcy;
    if (cell < 8.0f) cell = 8.0f;
    const float pad = 1.0f;

    for (int y = 0; y < gh; ++y) {
        for (int x = 0; x < gw; ++x) {
            const sim::Tile& t = world_->tile(x, y);
            ImU32 col;
            if (t.crop != sim::CropNone) {
                bool ripe = t.age >= sim::crop_maturity(t.crop);
                col = ripe ? IM_COL32(240, 200, 60, 255)   // gold
                           : IM_COL32(90, 170, 80, 255);   // green
            } else if (t.tilled) {
                col = IM_COL32(105, 75, 50, 255);          // tilled soil
            } else {
                col = IM_COL32(70, 100, 55, 255);          // wild
            }
            ImVec2 a{org.x + x * cell + pad, org.y + y * cell + pad};
            ImVec2 b{org.x + (x + 1) * cell - pad,
                     org.y + (y + 1) * cell - pad};
            dl->AddRectFilled(a, b, col, 3.0f);
        }
    }
    // drones
    for (int i = 0; i < world_->drones(); ++i) {
        ImVec2 c{org.x + (world_->drone_x(i) + 0.5f) * cell,
                 org.y + (world_->drone_y(i) + 0.5f) * cell};
        ImU32 col = (i == 0) ? IM_COL32(60, 140, 240, 255)
                             : IM_COL32(240, 120, 60, 255);
        dl->AddCircleFilled(c, cell * 0.30f, col);
        dl->AddCircle(c, cell * 0.30f, IM_COL32(255, 255, 255, 255), 0,
                      2.0f);
    }

    ImGui::Dummy({gw * cell, gh * cell});
    ImGui::End();
}

namespace {

const char* op_text(farm::lang::Tok t) {
    using farm::lang::Tok;
    switch (t) {
        case Tok::KwOr: return "or";   case Tok::KwAnd: return "and";
        case Tok::KwNot: return "not"; case Tok::Eq: return "==";
        case Tok::Ne: return "!=";     case Tok::Lt: return "<";
        case Tok::Le: return "<=";     case Tok::Gt: return ">";
        case Tok::Ge: return ">=";     case Tok::Plus: return "+";
        case Tok::Minus: return "-";   case Tok::Star: return "*";
        case Tok::Slash: return "/";   case Tok::Percent: return "%";
        default: return "?";
    }
}

std::string node_label(const farm::blueprint::Node& n) {
    using farm::blueprint::NK;
    switch (n.kind) {
        case NK::Entry: return "Entry";
        case NK::Assign: return "Set " + n.name;
        case NK::CallStmt: return n.name + "()";
        case NK::If: return "If";
        case NK::While: return "While";
        case NK::Repeat: return "Repeat";
        case NK::Return: return "Return";
        case NK::FuncDef: return "func " + n.name;
        case NK::IntLit: return std::to_string(n.ival);
        case NK::BoolLit: return n.bval ? "true" : "false";
        case NK::VarGet: return n.name;
        case NK::Unary: return op_text(n.op);
        case NK::Binary: return op_text(n.op);
        case NK::CallExpr: return n.name + "()";
    }
    return "?";
}

const farm::lang::Tok kBinOps[] = {
    farm::lang::Tok::Plus, farm::lang::Tok::Minus, farm::lang::Tok::Star,
    farm::lang::Tok::Slash, farm::lang::Tok::Percent, farm::lang::Tok::Eq,
    farm::lang::Tok::Ne, farm::lang::Tok::Lt, farm::lang::Tok::Le,
    farm::lang::Tok::Gt, farm::lang::Tok::Ge, farm::lang::Tok::KwAnd,
    farm::lang::Tok::KwOr};
const farm::lang::Tok kUnOps[] = {farm::lang::Tok::KwNot,
                                  farm::lang::Tok::Minus};

bool op_combo(const char* id, farm::lang::Tok& op, const farm::lang::Tok* set,
              int n) {
    int cur = 0;
    for (int i = 0; i < n; ++i)
        if (set[i] == op) cur = i;
    ImGui::SetNextItemWidth(70);
    if (ImGui::BeginCombo(id, op_text(op))) {
        bool changed = false;
        for (int i = 0; i < n; ++i) {
            bool sel = (i == cur);
            if (ImGui::Selectable(op_text(set[i]), sel)) {
                op = set[i];
                changed = true;
            }
        }
        ImGui::EndCombo();
        return changed;
    }
    return false;
}

// Attribute-id slots (unique per node: index*64 + slot).
inline int pin(int node_index, int slot) { return node_index * 64 + slot; }
constexpr int kExecIn = 1, kExecOut = 2, kBranchA = 3, kBranchB = 4,
              kValOut = 5, kValIn0 = 8;

bool has_exec_in(farm::blueprint::NK k) {
    using farm::blueprint::NK;
    return k == NK::Assign || k == NK::CallStmt || k == NK::If ||
           k == NK::While || k == NK::Repeat || k == NK::Return ||
           k == NK::FuncDef;
}

}  // namespace

void App::draw_blueprint() {
    ImGui::SetNextWindowPos({380, 560}, ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize({660, 230}, ImGuiCond_FirstUseEver);
    ImGui::Begin("Blueprint");

    if (ImGui::Button("Apply blueprint -> run")) {
        try {
            source_ = lang::print(blueprint::to_ast(graph_));
            rebuild();
        } catch (const std::exception& e) {
            error_ = e.what();
        }
    }
    ImGui::SameLine();
    ImGui::TextDisabled("(drag nodes; edit values -> code updates live)");

    bool edited = false;
    ImNodes::BeginNodeEditor();
    auto& ns = graph_.nodes;

    for (size_t i = 0; i < ns.size(); ++i) {
        blueprint::Node& n = ns[i];
        int ni = static_cast<int>(i);
        if (!graph_laid_)
            ImNodes::SetNodeGridSpacePos(ni, ImVec2(n.x, n.y));

        ImNodes::BeginNode(ni);
        ImGui::PushID(ni);
        ImNodes::BeginNodeTitleBar();
        ImGui::TextUnformatted(node_label(n).c_str());
        ImNodes::EndNodeTitleBar();

        // editable payload (the live blueprint -> code direction)
        using NK = blueprint::NK;
        if (n.kind == NK::IntLit) {
            int v = static_cast<int>(n.ival);
            ImGui::SetNextItemWidth(90);
            if (ImGui::InputInt("##i", &v)) { n.ival = v; edited = true; }
        } else if (n.kind == NK::BoolLit) {
            if (ImGui::Checkbox("##b", &n.bval)) edited = true;
        } else if (n.kind == NK::VarGet || n.kind == NK::Assign ||
                   n.kind == NK::CallStmt || n.kind == NK::CallExpr ||
                   n.kind == NK::FuncDef) {
            ImGui::SetNextItemWidth(120);
            if (ImGui::InputText("##nm", &n.name)) edited = true;
        } else if (n.kind == NK::Binary) {
            if (op_combo("##op", n.op, kBinOps,
                         (int)(sizeof(kBinOps) / sizeof(kBinOps[0]))))
                edited = true;
        } else if (n.kind == NK::Unary) {
            if (op_combo("##op", n.op, kUnOps,
                         (int)(sizeof(kUnOps) / sizeof(kUnOps[0]))))
                edited = true;
        }

        if (has_exec_in(n.kind)) {
            ImNodes::BeginInputAttribute(pin(ni, kExecIn));
            ImGui::TextUnformatted("in");
            ImNodes::EndInputAttribute();
        }
        for (size_t k = 0; k < n.in.size(); ++k) {
            ImNodes::BeginInputAttribute(pin(ni, kValIn0 + (int)k));
            ImGui::Text("arg%zu", k);
            ImNodes::EndInputAttribute();
        }
        // value output for expression nodes
        if (n.kind == blueprint::NK::IntLit ||
            n.kind == blueprint::NK::BoolLit ||
            n.kind == blueprint::NK::VarGet ||
            n.kind == blueprint::NK::Unary ||
            n.kind == blueprint::NK::Binary ||
            n.kind == blueprint::NK::CallExpr) {
            ImNodes::BeginOutputAttribute(pin(ni, kValOut));
            ImGui::TextUnformatted("val");
            ImNodes::EndOutputAttribute();
        }
        if (n.kind != blueprint::NK::IntLit &&
            n.kind != blueprint::NK::BoolLit &&
            n.kind != blueprint::NK::VarGet &&
            n.kind != blueprint::NK::Unary &&
            n.kind != blueprint::NK::Binary &&
            n.kind != blueprint::NK::CallExpr) {
            ImNodes::BeginOutputAttribute(pin(ni, kExecOut));
            ImGui::TextUnformatted("next");
            ImNodes::EndOutputAttribute();
            if (n.kind == blueprint::NK::If) {
                ImNodes::BeginOutputAttribute(pin(ni, kBranchA));
                ImGui::TextUnformatted("then");
                ImNodes::EndOutputAttribute();
                ImNodes::BeginOutputAttribute(pin(ni, kBranchB));
                ImGui::TextUnformatted("else");
                ImNodes::EndOutputAttribute();
            } else if (n.kind == blueprint::NK::While ||
                       n.kind == blueprint::NK::Repeat) {
                ImNodes::BeginOutputAttribute(pin(ni, kBranchA));
                ImGui::TextUnformatted("body");
                ImNodes::EndOutputAttribute();
            }
        }
        ImGui::PopID();
        ImNodes::EndNode();
    }

    int link_id = 0;
    for (size_t i = 0; i < ns.size(); ++i) {
        const blueprint::Node& n = ns[i];
        int ni = static_cast<int>(i);
        if (n.exec_next != -1)
            ImNodes::Link(link_id++, pin(ni, kExecOut),
                          pin(n.exec_next, kExecIn));
        if (n.body != -1)
            ImNodes::Link(link_id++, pin(ni, kBranchA),
                          pin(n.body, kExecIn));
        if (n.els != -1)
            ImNodes::Link(link_id++, pin(ni, kBranchB),
                          pin(n.els, kExecIn));
        for (size_t k = 0; k < n.in.size(); ++k)
            ImNodes::Link(link_id++, pin(n.in[k], kValOut),
                          pin(ni, kValIn0 + (int)k));
    }

    ImNodes::EndNodeEditor();

    // Capture user-dragged positions back into the graph + layout store so
    // they survive the next re-derive (layout keyed by AST id).
    for (size_t i = 0; i < ns.size(); ++i) {
        ImVec2 p = ImNodes::GetNodeGridSpacePos(static_cast<int>(i));
        ns[i].x = p.x;
        ns[i].y = p.y;
    }
    layout_ = blueprint::capture_layout(graph_);
    graph_laid_ = true;

    // Blueprint payload edit -> regenerate the code view (AST stays the
    // single source of truth; we do NOT reparse, so node ids are stable).
    if (edited) {
        try {
            source_ = lang::print(blueprint::to_ast(graph_));
        } catch (const std::exception& e) {
            error_ = e.what();
        }
    }
    ImGui::End();
}

void App::draw_tech() {
    ImGui::SetNextWindowPos({1050, 10}, ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize({220, 430}, ImGuiCond_FirstUseEver);
    ImGui::Begin("Tech");

    long long wheat =
        world_ ? static_cast<long long>(
                     world_->inventory_of(sim::CropWheat))
               : 0;
    ImGui::Text("Wheat: %lld", wheat);
    ImGui::TextDisabled("收获小麦换解锁");
    ImGui::Separator();

    for (int id = 0; id < sim::U_COUNT; ++id) {
        const sim::UnlockDef& d = sim::Progression::def(id);
        ImGui::PushID(id);
        if (prog_.is_unlocked(id)) {
            ImGui::TextDisabled("%s  [unlocked]", d.name);
        } else {
            ImGui::Text("%s  (%d)", d.name, d.cost_wheat);
            ImGui::SameLine();
            bool afford = world_ && wheat >= d.cost_wheat;
            if (!afford) ImGui::BeginDisabled();
            if (ImGui::SmallButton("Unlock") && world_ &&
                world_->spend(sim::CropWheat, d.cost_wheat)) {
                prog_.grant(id);
                src_dirty_ = true;  // re-validate editor w/ new features
            }
            if (!afford) ImGui::EndDisabled();
        }
        ImGui::PopID();
    }

    ImGui::Separator();
    ImGui::TextWrapped("也可在代码里调用 unlock(id) 自助解锁。");
    ImGui::End();
}

}  // namespace farm::game
