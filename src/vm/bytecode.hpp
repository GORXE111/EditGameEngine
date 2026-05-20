#pragma once
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

#include "lang/token.hpp"

// Stack-machine bytecode. A Chunk holds one Function per user function plus
// the synthetic top-level "@main". Variables resolve by name (globals /
// frame locals) so the VM matches the tree interpreter exactly; slot
// allocation is a later optimization.
namespace farm::vm {

enum class Op : uint8_t {
    PushInt,    // a = const-int index               -> push
    PushBool,   // a = 0/1                            -> push
    Load,       // a = name index                    -> push var
    Store,      // a = name index                    pop -> var
    Neg, Not,                                        // unary
    Add, Sub, Mul, Div, Mod,                         // int arithmetic
    Eq, Ne, Lt, Le, Gt, Ge,                          // comparison -> bool
    Jump,       // a = target ip
    JumpFalse,  // a = target ip; pop (must be bool)
    JumpTrue,   // a = target ip; pop (must be bool)
    Pop,        // discard top
    Call,       // a = name index, b = argc
    Ret,        // b = 1 if a value is on stack, else 0
    Halt,       // end of @main
    MakeList,   // a = element count; pop a -> push list
    IndexGet,   // pop idx, coll -> push coll[idx]
    IndexSet,   // pop value, idx, coll ; coll[idx] = value
};

struct Instr {
    Op op;
    int32_t a = 0;
    int32_t b = 0;
};

struct Function {
    std::string name;
    std::vector<std::string> params;
    std::vector<Instr> code;
};

struct Chunk {
    std::vector<int64_t> int_consts;
    std::vector<std::string> names;  // variable / function / native names
    std::unordered_map<std::string, int> func_index;  // name -> functions[]
    std::vector<Function> functions;  // [0] is always "@main"
};

}  // namespace farm::vm
