#include "game/recipes.hpp"

namespace farm::game {

const std::vector<Recipe>& recipes() {
    using sim::U_Carrot;
    using sim::U_Conditionals;
    using sim::U_Fertilizer;
    using sim::U_Loops;
    using sim::U_Repeat;
    static const std::vector<Recipe> r = {
        {"grow_wheat",
         "# 种这一格小麦\n"
         "till() plant(1)\n"
         "wait() wait() wait() wait() wait()\n"
         "harvest()\n",
         {}},
        {"step_east",
         "# 向东走一步\n"
         "move(1)\n",
         {}},
        {"fast_wheat",
         "# 化肥速成种小麦\n"
         "till() plant(1)\n"
         "fertilize()\n"
         "harvest()\n",
         {U_Fertilizer}},
        {"grow_carrot",
         "# 种这一格胡萝卜\n"
         "till() plant(2)\n"
         "wait() wait() wait() wait()\n"
         "wait() wait() wait() wait()\n"
         "harvest()\n",
         {U_Carrot}},
        {"smart_harvest",
         "# 智能收割：成熟才动手\n"
         "if can_harvest() then harvest() end\n",
         {U_Conditionals}},
        {"wait_then_harvest",
         "# 等成熟再收\n"
         "while can_harvest() == false do wait() end\n"
         "harvest()\n",
         {U_Loops}},
        {"walk_east_n",
         "# 向东走 4 格 (改数字调整距离)\n"
         "repeat 4 do move(1) end\n",
         {U_Repeat}},
        {"sweep_plant_wheat",
         "# 蛇形扫田 + 全场种小麦\n"
         "y = 0\n"
         "while y < 6 do\n"
         "  x = 0\n"
         "  while x < 8 do\n"
         "    till() plant(1)\n"
         "    wait() wait() wait() wait() wait()\n"
         "    harvest()\n"
         "    if x < 7 then move(1) end\n"
         "    x = x + 1\n"
         "  end\n"
         "  repeat 7 do move(3) end\n"
         "  if y < 5 then move(2) end\n"
         "  y = y + 1\n"
         "end\n",
         {U_Loops, U_Conditionals, U_Repeat}},
    };
    return r;
}

bool recipe_available(const Recipe& r, const sim::Progression& p) {
    for (int u : r.needs)
        if (!p.is_unlocked(u)) return false;
    return true;
}

}  // namespace farm::game
