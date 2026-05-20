#include "game/i18n.hpp"

#include <cstring>
#include <unordered_map>

namespace farm::game {
namespace {

Lang g_lang = Lang::ZH;

struct Pair { const char* zh; const char* en; };

const std::unordered_map<std::string, Pair>& table() {
    static const std::unordered_map<std::string, Pair> t = {
        // ---------- menu / panel titles ----------
        {"menu.settings",        {"设置",            "Settings"}},
        {"label.language",       {"语言",            "Language"}},
        {"lang.zh",              {"中文",            "Chinese"}},
        {"lang.en",              {"English",         "English"}},
        {"panel.control",        {"控制",            "Control"}},
        {"panel.code",           {"代码",            "Code"}},
        {"panel.farm",           {"农场",            "Farm"}},
        {"panel.blueprint",      {"蓝图",            "Blueprint"}},
        {"panel.tech",           {"科技",            "Tech"}},
        // ---------- buttons / labels ----------
        {"btn.run",              {"运行",            "Run"}},
        {"btn.pause",            {"暂停",            "Pause"}},
        {"btn.step",             {"单步",            "Step"}},
        {"btn.reset",            {"重置",            "Reset"}},
        {"btn.apply_reset",      {"应用并重置",      "Apply & Reset"}},
        {"btn.apply_bp",         {"应用蓝图 → 运行", "Apply Blueprint → Run"}},
        {"btn.unlock",           {"解锁",            "Unlock"}},
        {"label.speed",          {"速度",            "Speed"}},
        {"label.speed_unit",     {"%d 指令/帧",      "%d instr/frame"}},
        {"label.status",         {"状态",            "Status"}},
        {"label.tick",           {"时刻",            "Tick"}},
        {"label.drones",         {"机器人",          "Drones"}},
        {"label.drone_at",       {"  机器人 %d：(%d, %d)",
                                  "  drone %d: (%d, %d)"}},
        {"label.inventory",      {"小麦 %lld  胡萝卜 %lld  南瓜 %lld",
                                  "Wheat %lld  Carrot %lld  Pumpkin %lld"}},
        {"label.wheat",          {"小麦",            "Wheat"}},
        {"label.unlocked_tag",   {"[已解锁]",        "[unlocked]"}},
        {"label.no_world",       {"暂无世界",        "no world"}},
        {"hint.edit_apply",      {"(编辑后点 应用)", "(edit then Apply)"}},
        {"hint.bp_edit",         {"(拖动节点；改值即时同步代码)",
                                  "(drag nodes; edit values -> live code sync)"}},
        {"hint.unlock_code",     {"也可在代码里调用 unlock(id) 自助解锁。",
                                  "Or call unlock(id) from your code."}},
        {"hint.tech_subtitle",   {"收获小麦换解锁",  "Harvest wheat to unlock"}},
        // ---------- status ----------
        {"status.error",         {"错误",            "ERROR"}},
        {"status.no_program",    {"无程序",          "no program"}},
        {"status.completed",     {"已完成",          "completed"}},
        {"status.running",       {"运行中",          "running"}},
        {"status.paused",        {"暂停",            "paused"}},
        // ---------- node titles ----------
        {"node.entry",           {"起点",            "Entry"}},
        {"node.assign_prefix",   {"赋值: ",          "Set: "}},
        {"node.if",              {"如果",            "If"}},
        {"node.while",           {"当...时",          "While"}},
        {"node.repeat",          {"重复",            "Repeat"}},
        {"node.return",          {"返回",            "Return"}},
        {"node.funcdef_prefix",  {"函数: ",          "Function: "}},
        {"node.setindex",        {"改写元素",        "Set Element"}},
        {"node.intlit",          {"整数",            "Int"}},
        {"node.boollit",         {"布尔",            "Bool"}},
        {"node.varget",          {"取变量",          "Variable"}},
        {"node.unary",           {"一元运算",        "Unary"}},
        {"node.binary",          {"运算",            "Op"}},
        {"node.callstmt",        {"动作",            "Action"}},
        {"node.callexpr",        {"调用",            "Call"}},
        {"node.listlit",         {"列表",            "List"}},
        {"node.indexget",        {"取元素",          "Index"}},
        // ---------- pin role labels ----------
        {"pin.in",               {"进入",            "in"}},
        {"pin.next",             {"下一步",          "next"}},
        {"pin.then",             {"则",              "then"}},
        {"pin.else",             {"否则",            "else"}},
        {"pin.body",             {"循环体",          "body"}},
        {"pin.value_out",        {"值",              "val"}},
        {"pin.cond",             {"条件",            "cond"}},
        {"pin.count",            {"次数",            "count"}},
        {"pin.value",            {"值",              "value"}},
        {"pin.list",             {"列表",            "list"}},
        {"pin.index",            {"索引",            "index"}},
        {"pin.a",                {"a",               "a"}},
        {"pin.b",                {"b",               "b"}},
        {"pin.x",                {"x",               "x"}},
        {"pin.element",          {"元素",            "elem"}},
        {"pin.direction",        {"方向",            "direction"}},
        {"pin.crop",             {"作物",            "crop"}},
        {"pin.unlock_id",        {"解锁",            "unlock"}},
        {"pin.id",               {"编号",            "id"}},
        // ---------- native titles ----------
        {"native.move",          {"移动",            "Move"}},
        {"native.till",          {"开垦",            "Till"}},
        {"native.plant",         {"种植",            "Plant"}},
        {"native.water",         {"浇水",            "Water"}},
        {"native.fertilize",     {"施肥",            "Fertilize"}},
        {"native.harvest",       {"收割",            "Harvest"}},
        {"native.wait",          {"等待",            "Wait"}},
        {"native.sense",         {"感知地块",        "Sense Tile"}},
        {"native.pos",           {"坐标",            "Position"}},
        {"native.can_harvest",   {"可收割?",         "Can Harvest?"}},
        {"native.inventory",     {"库存",            "Inventory"}},
        {"native.num_items",     {"物品数量",        "Item Count"}},
        {"native.get_drone_id",  {"机器人ID",        "Drone ID"}},
        {"native.num_drones",    {"机器人数量",      "Drone Count"}},
        {"native.unlock",        {"解锁科技",        "Unlock Tech"}},
        {"native.get_cost",      {"解锁成本",        "Unlock Cost"}},
        {"native.num_unlocked",  {"是否已解锁",      "Is Unlocked"}},
        {"native.len",           {"长度",            "Length"}},
        {"native.append",        {"追加",            "Append"}},
        // ---------- tech (unlock) display names ----------
        {"tech.Conditionals",    {"条件 (if/else)",  "Conditionals (if/else)"}},
        {"tech.Loops",           {"循环 (while)",    "Loops (while)"}},
        {"tech.Repeat",          {"计次循环",        "Repeat"}},
        {"tech.Functions",       {"自定义函数",      "Functions"}},
        {"tech.Carrot",          {"胡萝卜",          "Carrot"}},
        {"tech.Expand",          {"扩张农田",        "Expand Farm"}},
        {"tech.Watering",        {"浇水加速",        "Watering"}},
        {"tech.Fertilizer",      {"化肥",            "Fertilizer"}},
        {"tech.Pumpkin",         {"南瓜",            "Pumpkin"}},
        {"tech.Polyculture",     {"伴生种植",        "Polyculture"}},
        {"tech.Drones",          {"多机协作",        "Drones"}},
        {"tech.Lists",           {"列表",            "Lists"}},
        // ---------- blueprint tabs / recipes ----------
        {"tab.recipes",          {"配方",            "Recipes"}},
        {"tab.graph",            {"节点图",          "Graph"}},
        {"btn.insert",           {"插入",            "Insert"}},
        {"hint.recipes",         {"挑一个配方点 插入：代码会追加到末尾，可继续编辑。",
                                  "Pick a recipe and click Insert — the snippet is appended to your code."}},
        {"recipe.locked_need",   {"需要解锁：",      "Needs: "}},
        // recipe titles + one-line descriptions
        {"recipe.grow_wheat.title",
         {"种这格小麦",                "Grow Wheat Here"}},
        {"recipe.grow_wheat.desc",
         {"开垦 + 种麦 + 等 5 tick + 收割。",
          "Till + plant wheat + wait 5 ticks + harvest."}},
        {"recipe.step_east.title",
         {"向东走一步",                "Step East"}},
        {"recipe.step_east.desc",
         {"机器人向东移动一格。",      "Move the drone one tile east."}},
        {"recipe.fast_wheat.title",
         {"化肥速成小麦",              "Fast Wheat (Fertilizer)"}},
        {"recipe.fast_wheat.desc",
         {"开垦 + 种麦 + 化肥瞬熟 + 收割。",
          "Till + plant + fertilize to ripen instantly + harvest."}},
        {"recipe.grow_carrot.title",
         {"种这格胡萝卜",              "Grow Carrot Here"}},
        {"recipe.grow_carrot.desc",
         {"开垦 + 种胡萝卜 + 等 8 tick + 收割。",
          "Till + plant carrot + wait 8 ticks + harvest."}},
        {"recipe.smart_harvest.title",
         {"智能收割",                  "Smart Harvest"}},
        {"recipe.smart_harvest.desc",
         {"只有成熟才动手，省 tick。",
          "Only harvest when ripe — saves ticks."}},
        {"recipe.wait_then_harvest.title",
         {"等成熟再收",                "Wait Then Harvest"}},
        {"recipe.wait_then_harvest.desc",
         {"原地等到当前作物成熟，然后收割。",
          "Wait in place until the crop ripens, then harvest."}},
        {"recipe.walk_east_n.title",
         {"向东走 N 格",               "Walk East N Steps"}},
        {"recipe.walk_east_n.desc",
         {"重复向东移动；默认 4 格，可改数字。",
          "Repeat eastward move; defaults to 4 — edit the number."}},
        {"recipe.sweep_plant_wheat.title",
         {"蛇形扫田 + 全场种小麦",      "Sweep Field & Plant Wheat"}},
        {"recipe.sweep_plant_wheat.desc",
         {"逐行蛇形走遍 8×6 农田，每格种麦收麦。",
          "Snake the drone over an 8×6 farm, planting and harvesting wheat in every tile."}},
    };
    return t;
}

}  // namespace

void Loc::set(Lang l) { g_lang = l; }
Lang Loc::get() { return g_lang; }

const char* Loc::tr(const char* key) {
    auto it = table().find(key);
    if (it == table().end()) return key;
    return (g_lang == Lang::ZH) ? it->second.zh : it->second.en;
}

const char* tech_name(const char* def_name) {
    static thread_local std::string buf;
    buf = "tech.";
    buf += def_name;
    const char* tr = Loc::tr(buf.c_str());
    return (tr == buf.c_str()) ? def_name : tr;  // fall back to raw def name
}

const char* native_title(const std::string& name) {
    static thread_local std::string buf;
    buf = "native.";
    buf += name;
    const char* tr = Loc::tr(buf.c_str());
    return (tr == buf.c_str()) ? name.c_str() : tr;
}

const char* native_pin(const std::string& name, int idx) {
    if (name == "move" && idx == 0) return Loc::tr("pin.direction");
    if ((name == "plant" || name == "inventory" || name == "num_items") &&
        idx == 0)
        return Loc::tr("pin.crop");
    if ((name == "unlock" || name == "get_cost" || name == "num_unlocked") &&
        idx == 0)
        return Loc::tr("pin.unlock_id");
    if (name == "append") return (idx == 0) ? Loc::tr("pin.list")
                                            : Loc::tr("pin.value");
    if (name == "len" && idx == 0) return Loc::tr("pin.list");
    static thread_local std::string buf;
    buf = "arg" + std::to_string(idx);
    return buf.c_str();
}

}  // namespace farm::game
