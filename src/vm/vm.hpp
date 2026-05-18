#pragma once
#include <string>

// Resumable bytecode VM (explicit frame stack + PC), modelled on UE5 FFrame.
// Engine-independent. M1 fills this in.
namespace farm::vm {

std::string version();

}  // namespace farm::vm
