#include "game/theme.hpp"

#include "imgui.h"

namespace farm::game {

namespace {

ImVec4 rgb(int r, int g, int b, float a = 1.f) {
    return ImVec4(r / 255.f, g / 255.f, b / 255.f, a);
}

void style_farmcode() {
    ImGuiStyle& s = ImGui::GetStyle();
    // shape / spacing
    s.WindowRounding   = 6.f;
    s.ChildRounding    = 6.f;
    s.FrameRounding    = 5.f;
    s.PopupRounding    = 5.f;
    s.GrabRounding     = 4.f;
    s.ScrollbarRounding = 8.f;
    s.TabRounding      = 4.f;
    s.WindowBorderSize = 0.f;
    s.FrameBorderSize  = 0.f;
    s.PopupBorderSize  = 0.f;
    s.WindowPadding    = ImVec2(12, 10);
    s.FramePadding     = ImVec2(10, 5);
    s.ItemSpacing      = ImVec2(8, 6);
    s.ItemInnerSpacing = ImVec2(6, 4);
    s.IndentSpacing    = 18.f;
    s.ScrollbarSize    = 14.f;
    s.GrabMinSize      = 12.f;
    s.WindowTitleAlign = ImVec2(0.f, 0.5f);

    // colour palette — earthy, warm, low-noise
    ImVec4* c = s.Colors;
    c[ImGuiCol_Text]                   = rgb(230, 230, 230);
    c[ImGuiCol_TextDisabled]           = rgb(122, 126, 133);
    c[ImGuiCol_WindowBg]               = rgb(29, 31, 37, 0.96f);
    c[ImGuiCol_ChildBg]                = rgb(0, 0, 0, 0.f);
    c[ImGuiCol_PopupBg]                = rgb(36, 38, 46, 0.98f);
    c[ImGuiCol_Border]                 = rgb(0, 0, 0, 0.f);
    c[ImGuiCol_BorderShadow]           = rgb(0, 0, 0, 0.f);
    c[ImGuiCol_FrameBg]                = rgb(39, 43, 51);
    c[ImGuiCol_FrameBgHovered]         = rgb(52, 57, 74);
    c[ImGuiCol_FrameBgActive]          = rgb(58, 63, 81);
    c[ImGuiCol_TitleBg]                = rgb(24, 26, 32);
    c[ImGuiCol_TitleBgActive]          = rgb(42, 46, 54);
    c[ImGuiCol_TitleBgCollapsed]       = rgb(24, 26, 32, 0.7f);
    c[ImGuiCol_MenuBarBg]              = rgb(28, 30, 36);
    c[ImGuiCol_ScrollbarBg]            = rgb(24, 26, 32, 0.7f);
    c[ImGuiCol_ScrollbarGrab]          = rgb(58, 63, 76);
    c[ImGuiCol_ScrollbarGrabHovered]   = rgb(80, 86, 102);
    c[ImGuiCol_ScrollbarGrabActive]    = rgb(112, 120, 140);
    c[ImGuiCol_CheckMark]              = rgb(212, 169, 62);    // amber
    c[ImGuiCol_SliderGrab]             = rgb(212, 169, 62);
    c[ImGuiCol_SliderGrabActive]       = rgb(230, 193, 86);
    c[ImGuiCol_Button]                 = rgb(50, 75, 52);      // mossy
    c[ImGuiCol_ButtonHovered]          = rgb(70, 105, 70);
    c[ImGuiCol_ButtonActive]           = rgb(95, 140, 95);
    c[ImGuiCol_Header]                 = rgb(50, 75, 52, 0.65f);
    c[ImGuiCol_HeaderHovered]          = rgb(70, 105, 70, 0.85f);
    c[ImGuiCol_HeaderActive]           = rgb(95, 140, 95);
    c[ImGuiCol_Separator]              = rgb(70, 75, 88, 0.4f);
    c[ImGuiCol_SeparatorHovered]       = rgb(212, 169, 62, 0.6f);
    c[ImGuiCol_SeparatorActive]        = rgb(212, 169, 62, 0.9f);
    c[ImGuiCol_ResizeGrip]             = rgb(212, 169, 62, 0.2f);
    c[ImGuiCol_ResizeGripHovered]      = rgb(212, 169, 62, 0.6f);
    c[ImGuiCol_ResizeGripActive]       = rgb(212, 169, 62, 0.9f);
    c[ImGuiCol_Tab]                    = rgb(36, 40, 48);
    c[ImGuiCol_TabHovered]             = rgb(70, 105, 70);
    c[ImGuiCol_TabActive]              = rgb(50, 75, 52);
    c[ImGuiCol_PlotLines]              = rgb(212, 169, 62);
    c[ImGuiCol_PlotHistogram]          = rgb(212, 169, 62);
    c[ImGuiCol_TextSelectedBg]         = rgb(212, 169, 62, 0.35f);
}

void load_font() {
    ImGuiIO& io = ImGui::GetIO();
    ImFontConfig cfg;
    cfg.OversampleH = 2;
    cfg.OversampleV = 1;
    cfg.PixelSnapH = false;
    // Microsoft YaHei has glyphs for Latin + ~all common CJK. Range is the
    // "ChineseSimplifiedCommon" subset (~2500 chars) — enough for our UI
    // and any reasonable comment/identifier the player types.
    const ImWchar* ranges =
        io.Fonts->GetGlyphRangesChineseSimplifiedCommon();
    ImFont* f = io.Fonts->AddFontFromFileTTF(
        "C:\\Windows\\Fonts\\msyh.ttc", 17.f, &cfg, ranges);
    if (!f)  // fall back to msyh light, then default
        f = io.Fonts->AddFontFromFileTTF(
            "C:\\Windows\\Fonts\\msyhl.ttc", 17.f, &cfg, ranges);
    if (f) io.FontDefault = f;  // else default ProggyClean (no CJK)
}

}  // namespace

void apply_theme() {
    load_font();
    style_farmcode();
}

}  // namespace farm::game
