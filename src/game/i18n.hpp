#pragma once
#include <string>

// Tiny i18n: keyed translation lookup with a runtime-switchable locale.
// Settings menu uses Loc::set(Lang); every UI string goes through tr(key).
// Unknown keys fall through to the key text so missing translations are
// obvious during development.
namespace farm::game {

enum class Lang { ZH, EN };

class Loc {
public:
    static void set(Lang l);
    static Lang get();
    static const char* tr(const char* key);
};

// Convenience helpers used by the blueprint UI.
const char* tech_name(const char* unlock_def_name);  // "Conditionals" -> 条件
const char* native_title(const std::string& callee_name);  // "plant" -> 种植
const char* native_pin(const std::string& callee_name, int arg_index);
const char* op_title(int /*Tok*/ op_int);  // operator-class human title

}  // namespace farm::game
