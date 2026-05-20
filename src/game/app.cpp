#include "game/app.hpp"

#include <algorithm>
#include <exception>

#include "imgui.h"
#include "imgui_stdlib.h"
#include "imnodes.h"
#include "blueprint/codec.hpp"
#include "game/i18n.hpp"
#include "lang/parser.hpp"
#include "lang/printer.hpp"
#include "vm/compiler.hpp"

namespace farm::game {

namespace {
// "Display###StableId" pattern: ImGui uses the part after ### as the
// stable window identity, so localized titles can change without losing
// position/size state across language switches.
const char* wtitle(const char* key, const char* stable_id) {
    static thread_local std::string buf;
    buf = Loc::tr(key);
    buf += "###";
    buf += stable_id;
    return buf.c_str();
}

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

    // top menu bar — Settings > Language
    if (ImGui::BeginMainMenuBar()) {
        if (ImGui::BeginMenu(Loc::tr("menu.settings"))) {
            if (ImGui::BeginMenu(Loc::tr("label.language"))) {
                Lang cur = Loc::get();
                if (ImGui::MenuItem(Loc::tr("lang.zh"), nullptr,
                                    cur == Lang::ZH))
                    Loc::set(Lang::ZH);
                if (ImGui::MenuItem(Loc::tr("lang.en"), nullptr,
                                    cur == Lang::EN))
                    Loc::set(Lang::EN);
                ImGui::EndMenu();
            }
            ImGui::EndMenu();
        }
        ImGui::EndMainMenuBar();
    }

    draw_controls();
    draw_editor();
    draw_farm();
    draw_blueprint();
    draw_tech();
}

void App::draw_controls() {
    ImGui::SetNextWindowPos({12, 34}, ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize({320, 246}, ImGuiCond_FirstUseEver);
    ImGui::Begin(wtitle("panel.control", "ctl"));

    const bool has_prog = !vms_.empty();
    const bool done = all_finished();
    if (ImGui::Button(running_ ? Loc::tr("btn.pause") : Loc::tr("btn.run")) &&
        has_prog)
        running_ = !running_;
    ImGui::SameLine();
    if (ImGui::Button(Loc::tr("btn.step"))) {
        running_ = false;
        step_one_action();
    }
    ImGui::SameLine();
    if (ImGui::Button(Loc::tr("btn.reset"))) rebuild();

    ImGui::SliderInt(Loc::tr("label.speed"), &speed_, 1, 4000,
                     Loc::tr("label.speed_unit"));

    const char* st;
    ImVec4 stc;
    if (!error_.empty()) {
        st = Loc::tr("status.error");
        stc = ImVec4(0.91f, 0.36f, 0.36f, 1);
    } else if (!has_prog) {
        st = Loc::tr("status.no_program");
        stc = ImVec4(0.55f, 0.55f, 0.55f, 1);
    } else if (done) {
        st = Loc::tr("status.completed");
        stc = ImVec4(0.40f, 0.80f, 0.70f, 1);
    } else if (running_) {
        st = Loc::tr("status.running");
        stc = ImVec4(0.50f, 0.75f, 0.50f, 1);
    } else {
        st = Loc::tr("status.paused");
        stc = ImVec4(0.83f, 0.66f, 0.24f, 1);
    }
    ImGui::Text("%s:", Loc::tr("label.status"));
    ImGui::SameLine();
    ImGui::TextColored(stc, "%s", st);
    if (world_) {
        ImGui::Text("%s: %llu  |  %s: %d", Loc::tr("label.tick"),
                    static_cast<unsigned long long>(world_->tick()),
                    Loc::tr("label.drones"), world_->drones());
        for (int i = 0; i < world_->drones(); ++i)
            ImGui::Text(Loc::tr("label.drone_at"), i, world_->drone_x(i),
                        world_->drone_y(i));
        ImGui::Text(Loc::tr("label.inventory"),
                    (long long)world_->inventory_of(sim::CropWheat),
                    (long long)world_->inventory_of(sim::CropCarrot),
                    (long long)world_->inventory_of(sim::CropPumpkin));
    }
    if (!error_.empty())
        ImGui::TextWrapped("%s", error_.c_str());
    ImGui::End();
}

void App::draw_editor() {
    ImGui::SetNextWindowPos({12, 548}, ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize({320, 240}, ImGuiCond_FirstUseEver);
    ImGui::Begin(wtitle("panel.code", "code"));
    if (ImGui::Button(Loc::tr("btn.apply_reset"))) rebuild();
    ImGui::SameLine();
    ImGui::TextDisabled("%s", Loc::tr("hint.edit_apply"));
    ImVec2 sz = ImGui::GetContentRegionAvail();
    if (ImGui::InputTextMultiline("##src", &source_, sz))
        src_dirty_ = true;  // live text -> blueprint sync next frame
    ImGui::End();
}

void App::draw_farm() {
    ImGui::SetNextWindowPos({344, 34}, ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize({522, 500}, ImGuiCond_FirstUseEver);
    ImGui::Begin(wtitle("panel.farm", "farm"));
    if (!world_) {
        ImGui::TextUnformatted(Loc::tr("label.no_world"));
        ImGui::End();
        return;
    }

    const int gw = world_->width(), gh = world_->height();
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 org = ImGui::GetCursorScreenPos();
    ImVec2 avail = ImGui::GetContentRegionAvail();
    float cell = avail.x / gw;
    float maxcy = avail.y / gh;
    if (maxcy < cell) cell = maxcy;
    if (cell < 12.0f) cell = 12.0f;

    // backdrop panel (slightly inset, soft border)
    const float backR = 8.f;
    ImVec2 bmin{org.x - 4, org.y - 4};
    ImVec2 bmax{org.x + gw * cell + 4, org.y + gh * cell + 4};
    dl->AddRectFilled(bmin, bmax, IM_COL32(20, 22, 26, 255), backR);
    dl->AddRect(bmin, bmax, IM_COL32(60, 65, 78, 200), backR, 0, 1.5f);

    const float pad = 1.5f;
    for (int y = 0; y < gh; ++y) {
        for (int x = 0; x < gw; ++x) {
            const sim::Tile& t = world_->tile(x, y);
            ImU32 top, bot;
            if (t.tilled) {
                top = IM_COL32(112, 80, 54, 255);   // tilled soil (warm)
                bot = IM_COL32(86, 60, 40, 255);
            } else {
                top = IM_COL32(86, 122, 70, 255);   // wild grass
                bot = IM_COL32(64, 96, 54, 255);
            }
            ImVec2 a{org.x + x * cell + pad, org.y + y * cell + pad};
            ImVec2 b{org.x + (x + 1) * cell - pad,
                     org.y + (y + 1) * cell - pad};
            dl->AddRectFilledMultiColor(a, b, top, top, bot, bot);
            // thin inner highlight at top — gives a 'card' feel
            dl->AddLine({a.x + 1, a.y + 1}, {b.x - 1, a.y + 1},
                        IM_COL32(255, 255, 255, 22), 1.0f);

            // watered dot (corner)
            if (t.watered)
                dl->AddCircleFilled({a.x + 6, a.y + 6}, 3.f,
                                    IM_COL32(120, 190, 235, 220));

            // crop sprite
            if (t.crop != sim::CropNone) {
                int m = sim::crop_maturity(t.crop);
                float g = m > 0 ? std::min(1.f, (float)t.age / (float)m)
                                : 1.f;
                bool ripe = (g >= 1.f);
                ImVec2 c{(a.x + b.x) * 0.5f, (a.y + b.y) * 0.5f};
                float r = cell * (0.16f + 0.22f * g);  // grows with age
                if (t.crop == sim::CropWheat) {
                    ImU32 col = ripe ? IM_COL32(238, 200, 70, 255)
                                     : IM_COL32(120, 175, 80, 255);
                    dl->AddCircleFilled(c, r, col, 20);
                } else if (t.crop == sim::CropCarrot) {
                    // small downward triangle (carrot)
                    ImU32 col = ripe ? IM_COL32(235, 132, 50, 255)
                                     : IM_COL32(140, 170, 80, 255);
                    dl->AddTriangleFilled({c.x - r, c.y - r},
                                          {c.x + r, c.y - r},
                                          {c.x, c.y + r * 1.2f}, col);
                } else if (t.crop == sim::CropPumpkin) {
                    ImU32 col = ripe ? IM_COL32(225, 110, 50, 255)
                                     : IM_COL32(150, 170, 75, 255);
                    dl->AddCircleFilled(c, r * 1.05f, col, 20);
                }
                if (ripe)  // mature halo
                    dl->AddCircle(c, r + 2.f, IM_COL32(255, 240, 180, 180),
                                  24, 1.8f);
            }
        }
    }

    // drones — rounded badge with drop shadow + stroke
    static const ImU32 drone_cols[] = {
        IM_COL32(70, 150, 240, 255),  // blue
        IM_COL32(240, 130, 60, 255),  // orange
        IM_COL32(190, 100, 220, 255), // violet
        IM_COL32(80, 200, 170, 255),  // teal
    };
    for (int i = 0; i < world_->drones(); ++i) {
        ImVec2 c{org.x + (world_->drone_x(i) + 0.5f) * cell,
                 org.y + (world_->drone_y(i) + 0.5f) * cell};
        float s = cell * 0.30f;
        ImVec2 a{c.x - s, c.y - s}, b{c.x + s, c.y + s};
        // shadow
        dl->AddRectFilled({a.x + 2, a.y + 3}, {b.x + 2, b.y + 3},
                          IM_COL32(0, 0, 0, 90), 5.f);
        dl->AddRectFilled(a, b,
                          drone_cols[i % (int)IM_ARRAYSIZE(drone_cols)],
                          5.f);
        dl->AddRect(a, b, IM_COL32(255, 255, 255, 220), 5.f, 0, 1.6f);
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
    using farm::game::Loc;
    using farm::game::native_title;
    switch (n.kind) {
        case NK::Entry:    return Loc::tr("node.entry");
        case NK::Assign:   return std::string(Loc::tr("node.assign_prefix")) +
                                  n.name;
        case NK::CallStmt: return native_title(n.name);
        case NK::If:       return Loc::tr("node.if");
        case NK::While:    return Loc::tr("node.while");
        case NK::Repeat:   return Loc::tr("node.repeat");
        case NK::Return:   return Loc::tr("node.return");
        case NK::FuncDef:  return std::string(Loc::tr("node.funcdef_prefix")) +
                                  n.name;
        case NK::IntLit:   return std::to_string(n.ival);
        case NK::BoolLit:  return n.bval ? "true" : "false";
        case NK::VarGet:   return n.name;
        case NK::Unary:    return op_text(n.op);
        case NK::Binary:   return op_text(n.op);
        case NK::CallExpr: return native_title(n.name);
        case NK::SetIndex: return Loc::tr("node.setindex");
        case NK::ListLit:  return Loc::tr("node.listlit");
        case NK::IndexGet: return Loc::tr("node.indexget");
    }
    return "?";
}

// Categorize a node for title-bar colouring. Helps the player tell
// gameplay actions, free queries, control-flow, math, and data apart.
enum class Cat {
    Action,    // tick-consuming robot action (warm red)
    Sense,     // free query / sensor (cool blue)
    Flow,      // control flow (purple)
    Op,        // arithmetic / logic (teal)
    Listy,     // list / index ops (green)
    Var,       // variable get/set (amber)
    Literal,   // constants (grey)
    Entry,     // entry node (neutral dark)
    Misc       // fallback
};

bool is_action_native(const std::string& n) {
    return n == "move" || n == "till" || n == "plant" || n == "water" ||
           n == "fertilize" || n == "harvest" || n == "wait" ||
           n == "unlock";
}
bool is_sense_native(const std::string& n) {
    return n == "sense" || n == "pos" || n == "can_harvest" ||
           n == "inventory" || n == "num_items" || n == "get_drone_id" ||
           n == "num_drones" || n == "get_cost" || n == "num_unlocked";
}
bool is_list_native(const std::string& n) {
    return n == "len" || n == "append";
}

Cat node_cat(const farm::blueprint::Node& n) {
    using farm::blueprint::NK;
    switch (n.kind) {
        case NK::Entry: return Cat::Entry;
        case NK::If: case NK::While: case NK::Repeat: case NK::Return:
        case NK::FuncDef: return Cat::Flow;
        case NK::Assign: case NK::VarGet: return Cat::Var;
        case NK::Unary: case NK::Binary: return Cat::Op;
        case NK::IntLit: case NK::BoolLit: return Cat::Literal;
        case NK::ListLit: case NK::IndexGet: case NK::SetIndex:
            return Cat::Listy;
        case NK::CallStmt: case NK::CallExpr:
            if (is_action_native(n.name)) return Cat::Action;
            if (is_sense_native(n.name)) return Cat::Sense;
            if (is_list_native(n.name)) return Cat::Listy;
            return Cat::Misc;
    }
    return Cat::Misc;
}

struct TitleCol { ImU32 bar, hov, sel; };
TitleCol cat_color(Cat c) {
    switch (c) {
        case Cat::Action:  return {IM_COL32(176, 70, 50, 255),
                                   IM_COL32(206,100, 75,255),
                                   IM_COL32(226,120, 95,255)};
        case Cat::Sense:   return {IM_COL32(48,108,170,255),
                                   IM_COL32(72,138,200,255),
                                   IM_COL32(96,162,224,255)};
        case Cat::Flow:    return {IM_COL32(108, 72,166,255),
                                   IM_COL32(134, 98,196,255),
                                   IM_COL32(160,124,222,255)};
        case Cat::Op:      return {IM_COL32( 58,140,124,255),
                                   IM_COL32( 84,170,154,255),
                                   IM_COL32(110,196,180,255)};
        case Cat::Listy:   return {IM_COL32( 96,148, 64,255),
                                   IM_COL32(120,176, 88,255),
                                   IM_COL32(146,202,114,255)};
        case Cat::Var:     return {IM_COL32(176,135, 58,255),
                                   IM_COL32(206,165, 88,255),
                                   IM_COL32(226,185,108,255)};
        case Cat::Literal: return {IM_COL32( 88, 96,112,255),
                                   IM_COL32(118,126,142,255),
                                   IM_COL32(146,154,170,255)};
        case Cat::Entry:   return {IM_COL32( 56, 60, 72,255),
                                   IM_COL32( 80, 84, 96,255),
                                   IM_COL32(104,108,120,255)};
        case Cat::Misc:    return {IM_COL32( 72, 78, 92,255),
                                   IM_COL32(100,106,120,255),
                                   IM_COL32(128,134,148,255)};
    }
    return {0, 0, 0};
}

// Pin label for the k-th value-input of a node, localized per kind/native.
const char* val_pin_label(const farm::blueprint::Node& n, int k) {
    using farm::blueprint::NK;
    using farm::game::Loc;
    using farm::game::native_pin;
    switch (n.kind) {
        case NK::Assign:   return Loc::tr("pin.value");
        case NK::If: case NK::While: return Loc::tr("pin.cond");
        case NK::Repeat:   return Loc::tr("pin.count");
        case NK::Return:   return Loc::tr("pin.value");
        case NK::Unary:    return Loc::tr("pin.x");
        case NK::Binary:   return (k == 0) ? Loc::tr("pin.a")
                                           : Loc::tr("pin.b");
        case NK::IndexGet:
            return (k == 0) ? Loc::tr("pin.list") : Loc::tr("pin.index");
        case NK::SetIndex:
            return (k == 0) ? Loc::tr("pin.list")
                 : (k == 1) ? Loc::tr("pin.index")
                            : Loc::tr("pin.value");
        case NK::ListLit:  return Loc::tr("pin.element");
        case NK::CallStmt: case NK::CallExpr:
            return native_pin(n.name, k);
        default:           return "";
    }
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
           k == NK::FuncDef || k == NK::SetIndex;
}

bool is_value_node(farm::blueprint::NK k) {
    using farm::blueprint::NK;
    return k == NK::IntLit || k == NK::BoolLit || k == NK::VarGet ||
           k == NK::Unary || k == NK::Binary || k == NK::CallExpr ||
           k == NK::ListLit || k == NK::IndexGet;
}

}  // namespace

void App::draw_blueprint() {
    ImGui::SetNextWindowPos({876, 34}, ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize({392, 754}, ImGuiCond_FirstUseEver);
    ImGui::Begin(wtitle("panel.blueprint", "bp"));

    if (ImGui::Button(Loc::tr("btn.apply_bp"))) {
        try {
            source_ = lang::print(blueprint::to_ast(graph_));
            rebuild();
        } catch (const std::exception& e) {
            error_ = e.what();
        }
    }
    ImGui::SameLine();
    ImGui::TextDisabled("%s", Loc::tr("hint.bp_edit"));

    bool edited = false;
    ImNodes::BeginNodeEditor();
    auto& ns = graph_.nodes;

    for (size_t i = 0; i < ns.size(); ++i) {
        blueprint::Node& n = ns[i];
        int ni = static_cast<int>(i);
        if (!graph_laid_)
            ImNodes::SetNodeGridSpacePos(ni, ImVec2(n.x, n.y));

        TitleCol tc = cat_color(node_cat(n));
        ImNodes::PushColorStyle(ImNodesCol_TitleBar, tc.bar);
        ImNodes::PushColorStyle(ImNodesCol_TitleBarHovered, tc.hov);
        ImNodes::PushColorStyle(ImNodesCol_TitleBarSelected, tc.sel);

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
            ImGui::TextUnformatted(Loc::tr("pin.in"));
            ImNodes::EndInputAttribute();
        }
        for (size_t k = 0; k < n.in.size(); ++k) {
            ImNodes::BeginInputAttribute(pin(ni, kValIn0 + (int)k));
            ImGui::TextUnformatted(val_pin_label(n, (int)k));
            ImNodes::EndInputAttribute();
        }
        // value output for expression nodes
        if (is_value_node(n.kind)) {
            ImNodes::BeginOutputAttribute(pin(ni, kValOut));
            ImGui::TextUnformatted(Loc::tr("pin.value_out"));
            ImNodes::EndOutputAttribute();
        }
        if (!is_value_node(n.kind)) {  // exec node (incl. Entry)
            ImNodes::BeginOutputAttribute(pin(ni, kExecOut));
            ImGui::TextUnformatted(Loc::tr("pin.next"));
            ImNodes::EndOutputAttribute();
            if (n.kind == blueprint::NK::If) {
                ImNodes::BeginOutputAttribute(pin(ni, kBranchA));
                ImGui::TextUnformatted(Loc::tr("pin.then"));
                ImNodes::EndOutputAttribute();
                ImNodes::BeginOutputAttribute(pin(ni, kBranchB));
                ImGui::TextUnformatted(Loc::tr("pin.else"));
                ImNodes::EndOutputAttribute();
            } else if (n.kind == blueprint::NK::While ||
                       n.kind == blueprint::NK::Repeat) {
                ImNodes::BeginOutputAttribute(pin(ni, kBranchA));
                ImGui::TextUnformatted(Loc::tr("pin.body"));
                ImNodes::EndOutputAttribute();
            }
        }
        ImGui::PopID();
        ImNodes::EndNode();
        ImNodes::PopColorStyle();
        ImNodes::PopColorStyle();
        ImNodes::PopColorStyle();
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
    ImGui::SetNextWindowPos({12, 288}, ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize({320, 252}, ImGuiCond_FirstUseEver);
    ImGui::Begin(wtitle("panel.tech", "tech"));

    long long wheat =
        world_ ? static_cast<long long>(
                     world_->inventory_of(sim::CropWheat))
               : 0;
    ImGui::Text("%s: %lld", Loc::tr("label.wheat"), wheat);
    ImGui::TextDisabled("%s", Loc::tr("hint.tech_subtitle"));
    ImGui::Separator();

    for (int id = 0; id < sim::U_COUNT; ++id) {
        const sim::UnlockDef& d = sim::Progression::def(id);
        ImGui::PushID(id);
        if (prog_.is_unlocked(id)) {
            ImGui::TextDisabled("%s  %s", tech_name(d.name),
                                Loc::tr("label.unlocked_tag"));
        } else {
            ImGui::Text("%s  (%d)", tech_name(d.name), d.cost_wheat);
            ImGui::SameLine();
            bool afford = world_ && wheat >= d.cost_wheat;
            if (!afford) ImGui::BeginDisabled();
            if (ImGui::SmallButton(Loc::tr("btn.unlock")) && world_ &&
                world_->spend(sim::CropWheat, d.cost_wheat)) {
                prog_.grant(id);
                src_dirty_ = true;  // re-validate editor w/ new features
            }
            if (!afford) ImGui::EndDisabled();
        }
        ImGui::PopID();
    }

    ImGui::Separator();
    ImGui::TextWrapped("%s", Loc::tr("hint.unlock_code"));
    ImGui::End();
}

}  // namespace farm::game
