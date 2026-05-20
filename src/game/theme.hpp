#pragma once

// One-call visual setup: load a CJK-capable font and apply the FarmCode
// color/spacing theme. Call after ImGui::CreateContext(), before the
// first ImGui::NewFrame.
namespace farm::game {

void apply_theme();

}  // namespace farm::game
