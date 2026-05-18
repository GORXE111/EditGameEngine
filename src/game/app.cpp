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
const char* kExample =
    "# FarmCode 示例：机器人蛇形扫过农田，逐格种小麦\n"
    "y = 0\n"
    "while y < 6 do\n"
    "  x = 0\n"
    "  while x < 8 do\n"
    "    till()\n"
    "    plant(1)\n"
    "    repeat 5 do wait() end\n"
    "    harvest()\n"
    "    if x < 7 then move(1) end\n"
    "    x = x + 1\n"
    "  end\n"
    "  repeat 7 do move(3) end\n"   // back to west edge
    "  if y < 5 then move(2) end\n" // next row
    "  y = y + 1\n"
    "end\n";
}  // namespace

App::App() {
    source_ = kExample;
    rebuild();
}

void App::rebuild() {
    error_.clear();
    running_ = false;
    vm_.reset();
    try {
        lang::Program prog = lang::parse(source_);
        graph_ = blueprint::build(prog);  // blueprint view of this program
        graph_laid_ = false;
        chunk_ = std::make_unique<vm::Chunk>(vm::compile(prog));
        world_ = std::make_unique<sim::World>(kW, kH);
        host_ = std::make_unique<sim::RobotHost>(*world_);
        vm_ = std::make_unique<vm::VM>(*chunk_, *host_);
    } catch (const std::exception& e) {
        error_ = e.what();
    }
}

void App::advance(int64_t budget) {
    if (!vm_ || vm_->finished()) return;
    try {
        vm_->run(budget);
    } catch (const std::exception& e) {
        error_ = e.what();
        running_ = false;
    }
}

void App::step_one_action() {
    if (!vm_ || vm_->finished()) return;
    uint64_t t0 = world_->tick();
    for (int guard = 0; guard < 100000; ++guard) {
        advance(64);
        if (!error_.empty() || vm_->finished()) break;
        if (world_->tick() != t0) break;
    }
}

void App::frame() {
    if (running_) advance(speed_);
    draw_controls();
    draw_editor();
    draw_farm();
    draw_blueprint();
}

void App::draw_controls() {
    ImGui::SetNextWindowPos({10, 10}, ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize({360, 230}, ImGuiCond_FirstUseEver);
    ImGui::Begin("Control");

    const bool done = !vm_ || vm_->finished();
    if (ImGui::Button(running_ ? "Pause" : "Run") && vm_) running_ = !running_;
    ImGui::SameLine();
    if (ImGui::Button("Step")) { running_ = false; step_one_action(); }
    ImGui::SameLine();
    if (ImGui::Button("Reset")) rebuild();

    ImGui::SliderInt("Speed", &speed_, 1, 4000, "%d instr/frame");

    const char* st = !error_.empty() ? "ERROR"
                     : !vm_          ? "no program"
                     : done          ? "completed"
                     : running_      ? "running"
                                     : "paused";
    ImGui::Text("Status: %s", st);
    if (world_) {
        ImGui::Text("Tick: %llu",
                    static_cast<unsigned long long>(world_->tick()));
        ImGui::Text("Robot: (%d, %d)", world_->rx(), world_->ry());
        ImGui::Text("Wheat harvested: %lld",
                    static_cast<long long>(
                        world_->inventory_of(sim::CropWheat)));
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
    ImGui::InputTextMultiline("##src", &source_, sz);
    ImGui::End();
}

void App::draw_farm() {
    ImGui::SetNextWindowPos({380, 10}, ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize({660, 540}, ImGuiCond_FirstUseEver);
    ImGui::Begin("Farm");
    if (!world_) { ImGui::TextUnformatted("no world"); ImGui::End(); return; }

    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 org = ImGui::GetCursorScreenPos();
    ImVec2 avail = ImGui::GetContentRegionAvail();
    float cell = avail.x / kW;
    float maxcy = avail.y / kH;
    if (maxcy < cell) cell = maxcy;
    if (cell < 8.0f) cell = 8.0f;
    const float pad = 1.0f;

    for (int y = 0; y < kH; ++y) {
        for (int x = 0; x < kW; ++x) {
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
    // robot
    ImVec2 c{org.x + (world_->rx() + 0.5f) * cell,
             org.y + (world_->ry() + 0.5f) * cell};
    dl->AddCircleFilled(c, cell * 0.30f, IM_COL32(60, 140, 240, 255));
    dl->AddCircle(c, cell * 0.30f, IM_COL32(255, 255, 255, 255), 0, 2.0f);

    ImGui::Dummy({kW * cell, kH * cell});
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

    if (ImGui::Button("Regenerate code from blueprint")) {
        try {
            source_ = lang::print(blueprint::to_ast(graph_));
            rebuild();
        } catch (const std::exception& e) {
            error_ = e.what();
        }
    }
    ImGui::SameLine();
    ImGui::TextDisabled("(read-only view; editing arrives in M5)");

    ImNodes::BeginNodeEditor();
    const auto& ns = graph_.nodes;

    for (size_t i = 0; i < ns.size(); ++i) {
        const blueprint::Node& n = ns[i];
        int ni = static_cast<int>(i);
        if (!graph_laid_)
            ImNodes::SetNodeGridSpacePos(ni, ImVec2(n.x, n.y));

        ImNodes::BeginNode(ni);
        ImNodes::BeginNodeTitleBar();
        ImGui::TextUnformatted(node_label(n).c_str());
        ImNodes::EndNodeTitleBar();

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
    graph_laid_ = true;
    ImGui::End();
}

}  // namespace farm::game
