#include "vm/vm.hpp"

#include "lang/parser.hpp"
#include "vm/compiler.hpp"

namespace farm::vm {
using farm::lang::RuntimeError;
using farm::lang::Value;

std::string version() { return "vm-v0"; }

namespace {
constexpr size_t kMaxCallDepth = 4000;
[[noreturn]] void rterr(const std::string& m) { throw RuntimeError(m); }
}  // namespace

VM::VM(const Chunk& chunk, farm::lang::Host& host)
    : c_(chunk), host_(host) {
    Frame top;
    top.fn = &c_.functions[0];  // @main
    top.is_global = true;
    frames_.push_back(std::move(top));
}

Value VM::pop() {
    if (stack_.empty()) rterr("vm stack underflow");
    Value v = stack_.back();
    stack_.pop_back();
    return v;
}

Value* VM::resolve(Frame& f, const std::string& name) {
    if (!f.is_global) {
        auto it = f.locals.find(name);
        if (it != f.locals.end()) return &it->second;
    }
    auto g = globals_.find(name);
    if (g != globals_.end()) return &g->second;
    return nullptr;
}

void VM::do_store(Frame& f, const std::string& name, Value v) {
    if (!f.is_global) {
        auto it = f.locals.find(name);
        if (it != f.locals.end()) { it->second = v; return; }
    }
    auto g = globals_.find(name);
    if (g != globals_.end()) { g->second = v; return; }
    if (f.is_global) globals_[name] = v;
    else f.locals[name] = v;
}

void VM::exec_binary(Op op) {
    Value r = pop();
    Value l = pop();
    if (op == Op::Eq || op == Op::Ne) {
        if (l.t != r.t) rterr("'==' / '!=' requires same-type operands");
        bool eq = (l == r);
        push(Value::B(op == Op::Eq ? eq : !eq));
        return;
    }
    if (!l.is_int() || !r.is_int())
        rterr("arithmetic/comparison requires int operands");
    switch (op) {
        case Op::Add: push(Value::I(l.i + r.i)); break;
        case Op::Sub: push(Value::I(l.i - r.i)); break;
        case Op::Mul: push(Value::I(l.i * r.i)); break;
        case Op::Div:
            if (r.i == 0) rterr("division by zero");
            push(Value::I(l.i / r.i));
            break;
        case Op::Mod:
            if (r.i == 0) rterr("modulo by zero");
            push(Value::I(l.i % r.i));
            break;
        case Op::Lt: push(Value::B(l.i < r.i)); break;
        case Op::Le: push(Value::B(l.i <= r.i)); break;
        case Op::Gt: push(Value::B(l.i > r.i)); break;
        case Op::Ge: push(Value::B(l.i >= r.i)); break;
        default: rterr("bad binary opcode");
    }
}

bool VM::builtin(const std::string& name, const std::vector<Value>& args,
                 Value& out) {
    if (name == "len") {
        if (args.size() != 1 || !args[0].is_list())
            rterr("len(list) expected");
        out = Value::I(static_cast<int64_t>(args[0].list->size()));
        return true;
    }
    if (name == "append") {
        if (args.size() != 2 || !args[0].is_list())
            rterr("append(list, value) expected");
        args[0].list->push_back(args[1]);
        out = Value::I(0);
        return true;
    }
    return false;
}

VM::RunResult VM::run(int64_t budget) {
    uint64_t steps = 0;
    if (finished_) return {Status::Completed, result_, 0};

    while (!finished_) {
        if (budget == 0) return {Status::Paused, Value{}, steps};
        if (budget > 0) --budget;
        ++steps;

        Frame& fr = frames_.back();
        const Instr& in = fr.fn->code[fr.ip++];

        switch (in.op) {
            case Op::PushInt:
                push(Value::I(c_.int_consts[in.a]));
                break;
            case Op::PushBool:
                push(Value::B(in.a != 0));
                break;
            case Op::Load: {
                const std::string& nm = c_.names[in.a];
                Value* v = resolve(fr, nm);
                if (!v) rterr("use of undefined variable '" + nm + "'");
                push(*v);
                break;
            }
            case Op::Store:
                do_store(fr, c_.names[in.a], pop());
                break;
            case Op::Neg: {
                Value v = pop();
                if (!v.is_int()) rterr("unary '-' requires an int");
                push(Value::I(-v.i));
                break;
            }
            case Op::Not: {
                Value v = pop();
                if (!v.is_bool()) rterr("'not' requires a bool");
                push(Value::B(!v.boolean()));
                break;
            }
            case Op::Add: case Op::Sub: case Op::Mul: case Op::Div:
            case Op::Mod: case Op::Eq: case Op::Ne: case Op::Lt:
            case Op::Le: case Op::Gt: case Op::Ge:
                exec_binary(in.op);
                break;
            case Op::Jump:
                fr.ip = static_cast<size_t>(in.a);
                break;
            case Op::JumpFalse: {
                Value v = pop();
                if (!v.is_bool()) rterr("condition must be bool");
                if (!v.boolean()) fr.ip = static_cast<size_t>(in.a);
                break;
            }
            case Op::JumpTrue: {
                Value v = pop();
                if (!v.is_bool()) rterr("condition must be bool");
                if (v.boolean()) fr.ip = static_cast<size_t>(in.a);
                break;
            }
            case Op::Pop:
                pop();
                break;
            case Op::Call: {
                const std::string& nm = c_.names[in.a];
                int argc = in.b;
                if (nm == "len" || nm == "append") {  // language builtins
                    std::vector<Value> args(argc);
                    for (int i = argc - 1; i >= 0; --i) args[i] = pop();
                    Value b;
                    builtin(nm, args, b);
                    push(b);
                    break;
                }
                auto fit = c_.func_index.find(nm);
                if (fit == c_.func_index.end()) {
                    // native
                    std::vector<Value> args(argc);
                    for (int i = argc - 1; i >= 0; --i) args[i] = pop();
                    push(host_.call(nm, args));
                    break;
                }
                const Function* callee = &c_.functions[fit->second];
                if (static_cast<int>(callee->params.size()) != argc)
                    rterr("function '" + nm + "' expects " +
                          std::to_string(callee->params.size()) +
                          " args, got " + std::to_string(argc));
                if (frames_.size() >= kMaxCallDepth)
                    rterr("call depth exceeded (runaway recursion?)");
                std::vector<Value> args(argc);
                for (int i = argc - 1; i >= 0; --i) args[i] = pop();
                Frame nf;
                nf.fn = callee;
                nf.is_global = false;
                for (size_t i = 0; i < callee->params.size(); ++i)
                    nf.locals[callee->params[i]] = args[i];
                frames_.push_back(std::move(nf));
                break;
            }
            case Op::Ret: {
                Value rv = (in.b != 0) ? pop() : Value::I(0);
                frames_.pop_back();
                if (frames_.empty()) {
                    finished_ = true;
                    result_ = rv;
                } else {
                    push(rv);
                }
                break;
            }
            case Op::MakeList: {
                auto v = std::make_shared<std::vector<Value>>(
                    static_cast<size_t>(in.a));
                for (int k = in.a - 1; k >= 0; --k)
                    (*v)[static_cast<size_t>(k)] = pop();
                push(Value::L(std::move(v)));
                break;
            }
            case Op::IndexGet: {
                Value ix = pop();
                Value c = pop();
                if (!c.is_list()) rterr("indexing requires a list");
                if (!ix.is_int()) rterr("list index must be an int");
                if (ix.i < 0 ||
                    ix.i >= static_cast<int64_t>(c.list->size()))
                    rterr("list index out of range");
                push((*c.list)[static_cast<size_t>(ix.i)]);
                break;
            }
            case Op::IndexSet: {
                Value val = pop();
                Value ix = pop();
                Value c = pop();
                if (!c.is_list())
                    rterr("indexed assignment requires a list");
                if (!ix.is_int()) rterr("list index must be an int");
                if (ix.i < 0 ||
                    ix.i >= static_cast<int64_t>(c.list->size()))
                    rterr("list index out of range");
                (*c.list)[static_cast<size_t>(ix.i)] = val;
                break;
            }
            case Op::Halt:
                finished_ = true;
                result_ = Value::I(0);
                break;
        }
    }
    return {Status::Completed, result_, steps};
}

Value execute(const farm::lang::Program& program, farm::lang::Host& host) {
    Chunk ch = compile(program);
    VM vm(ch, host);
    return vm.run().value;
}

Value execute(const std::string& source, farm::lang::Host& host) {
    return execute(farm::lang::parse(source), host);
}

}  // namespace farm::vm
