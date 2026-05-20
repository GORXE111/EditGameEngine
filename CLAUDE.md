# 编程农场游戏 — 项目入口

> 新会话先读本文件，再读 `.claude/rules/requirements.md` 与 `.claude/rules/references.md`。

## 项目目标

编程驱动的农场游戏：玩家用**蓝图或文本代码**编写程序，指挥机器人在农田网格自动种田。编程=核心玩法（对标 *The Farmer Was Replaced*）。自研一门嵌入式脚本语言 + 字节码虚拟机（类 Lua），蓝图与代码双向同构（同一份 AST）。

## 技术栈

C++20 / 自研引擎 / SDL2 + OpenGL / Dear ImGui + imnodes / CMake + Ninja + MSVC。详见 `.claude/rules/requirements.md` 第 2 节。

## 关键约束

- **UE5 仓库 `C:\Users\admin\Documents\UE5NEWAI` 只读**，仅查阅参考，绝不修改。见 `.claude/rules/references.md`。
- `lang`/`vm`/`sim` 无引擎依赖，必须可无头单测。
- VM 可恢复执行（显式帧栈，对标 UE5 FFrame）。

## 目录结构

```
.claude/rules/       requirements.md, references.md
CMakeLists.txt       顶层；CMakePresets.json（Ninja+MSVC @ D:\VS2022）
third_party/         imgui, imnodes（内嵌）；SDL2/doctest 走 FetchContent
src/lang/            Lexer/Parser/AST/编译器→字节码（无引擎依赖）
src/vm/              字节码 VM、可恢复执行、原生函数绑定（无引擎依赖）
src/blueprint/       节点模型 + AST↔图 编解码 + 布局持久化
src/sim/             农场世界/网格/作物/经济/tick + 原生 API（无引擎依赖）
src/game/            SDL2+OpenGL+ImGui+imnodes 渲染/HUD/编辑器/运行控制
tests/               doctest 无头黄金测试
```

## 构建命令

```
cmake --preset default
cmake --build --preset default
ctest --preset default        # 无头测试
```

MSVC 工具链：VS2022 Community @ `D:\VS2022`（非默认路径）。
远端：`origin` = https://github.com/GORXE111/EditGameEngine （仅用户明确要求时推送）。
已知后续：imgui 当前为 release tag v1.91.5；M3 多面板布局时切换到 docking 分支并固定提交。

## 里程碑状态

- [x] M0 地基（规则/CMake/起窗）  ← 构建+无头测试通过；窗口待人工目视
- [x] M1 语言与 VM 核心  ← 词法/语法/打印/树解释器/字节码VM，25用例114断言全过
- [x] M2 农场仿真 + 原生 API  ← 网格/作物生长/机器人/原生API，黄金程序29用例134断言全过
- [x] M3 渲染 + 首个可玩  ← SDL2/ImGui 农场视图+代码编辑器+运行控制，首个可玩
- [x] M4 蓝图编辑器  ← AST↔图编解码(往返等价)+imnodes可视化，32用例158断言全过
- [x] M5 双向同构同步  ← 文本↔蓝图实时互转+布局按id保留+节点改值回写，35用例171断言全过
- [x] M6 进程与内容  ← TFWR式科技树+经济+语言特性渐进解锁，40用例192断言全过

**M0–M6 全部完成。** 计划外增强：

- [x] E1 TFWR 机制深化  ← 浇水x2生长/化肥瞬熟/南瓜(高产)/伴生加成，46用例213断言全过
- [x] E2 共享程序多无人机  ← World多drone+RobotHost手动tick+Fleet调度+get_drone_id()，51用例229断言全过
- [x] E3 语言 v2 — 数组/列表  ← 列表/下标/SetIndex/len/append + U_Lists 门控 + 蓝图往返，58用例267断言全过
- [x] P1 UI 与画面打磨  ← Microsoft YaHei CJK字体 + FarmCode暖系主题 + 农场重绘(渐变格/作物形状/成熟光晕/机器人徽章+阴影) + 五面板布局重排 + 状态彩色
- [x] P2 本地化 + 蓝图玩家友好化  ← i18n 模块(Settings→Language 中文/English) + 节点人话标题/按角色着色(动作/感知/控制流/运算/列表/字面量/变量) + 输入针语义化("作物"/"条件"不再是 arg0) + 科技/状态/按钮/面板全本地化
- [x] P3 配方库(Recipe Library)  ← 蓝图面板加 Tab(配方/节点图)；玩家从配方列表点"插入"即追加可运行片段；按所需解锁自动灰；起步配方：种小麦/胡萝卜/化肥速成/扫田/智能收割/等成熟/走N格 等 8 个
- [x] E4.1 UE 风结构编辑  ← 右键画布开调色板(8 类别)生新节点；拖针↔针自由连线(类型/方向校验)；选中按 Delete 删节点/线；Apply 走 compact+to_ast，缺针/死引用给清晰错误

测试累计 63 用例 / 284 断言全绿。

> E2：World 多无人机(drone0 向后兼容单机)；RobotHost 加 drone 索引+手动tick模式；run_fleet 轮流调度(每轮各无人机一动作后世界推进一tick)；get_drone_id()/num_drones() 原生；U_Drones 解锁(2机)；App 统一为 N 架机队(N=1 等价原单机)。

> M5 范围：双向同构覆盖「文本↔蓝图重建(布局按 ast_id 保留)」与「蓝图节点改值/改名/改运算符→实时回写」。蓝图结构性编辑(增删节点/重连)留待后续。
> M6 范围：科技树解锁作物(Carrot)/农田扩张/原生API/语言特性(if/while/repeat/func 渐进解锁)+经济(收获换解锁)+unlock()/get_cost()/num_items()/num_unlocked() 原生。语言数组等属 v2，门控待 v2。

## 开发规范

Commit：`type(scope): description`，scope ∈ lang|vm|blueprint|sim|game|infra。仅用户明确要求时提交。详见 `.claude/rules/requirements.md` 第 6 节。
