# 需求规则（最终基线 — 不可漂移）

> 本文件是经多轮需求澄清后锁定的产品与技术基线。任何开发不得偏离；如需变更，先改本文件再改代码。

## 1. 项目目标与玩法定义

做一款**编程驱动的农场游戏**：玩家用**蓝图**或**文本代码**编写程序，指挥机器人在农田网格上自动种植/浇水/收割。编程本身就是核心玩法（对标 *The Farmer Was Replaced*）。

- 玩法手感基线：永续自动化循环（`while true`）、世界随 tick 持续推进、机器人指令式 API。
- 玩家产出的是"程序"，不是手动操作；程序质量决定农场产出。

## 2. 技术栈决策表（锁定）

| 维度 | 决策 |
|---|---|
| 引擎语言 | C++20，游戏核心循环自研（不用 Unity/Godot/UE） |
| 引擎底座 | SDL2（窗口/输入/GL 上下文）+ OpenGL + 自写 2D 渲染/循环/ECS/仿真 |
| 编辑器 UI | Dear ImGui（SDL2+OpenGL3 后端）+ imnodes（节点画布） |
| 脚本系统 | 自研嵌入式脚本语言 + 字节码虚拟机（类 Lua），游戏内运行 |
| 可视化形态 | 节点+连线图（Unreal Blueprint 风） |
| 蓝图↔代码 | 双向同构：同一份 AST/字节码，蓝图与代码是同一程序的两个视图 |
| 构建 | CMake + Ninja + MSVC（VS2022 @ `D:\VS2022`），CMakePresets.json |
| 依赖 | SDL2/doctest 走 CMake FetchContent；imgui/imnodes 源码内嵌 `third_party/` |
| 测试 | doctest，无头黄金测试 |
| 引擎参考 | UE5 源码 `C:\Users\admin\Documents\UE5NEWAI`（**只读**，见 references.md） |

## 3. 架构契约

- **统一 AST 是唯一真源**。文本代码与蓝图节点图都是 AST 的投影。
- 每个 AST 节点带稳定 ID；蓝图节点坐标存于以节点 ID 为键的旁路布局表，随存档持久化。
- VM 执行编译后的字节码，不重新解释文本。
- `lang` / `vm` / `sim` 三个静态库**无引擎依赖**，必须可无头单测。
- VM 必须**可恢复执行**：每个机器人原生动作执行后挂起、交还控制权，由游戏 tick 恢复；支持单步/暂停/高亮当前指令。实现为显式 VM 状态机（PC + 显式帧栈），对标 UE5 `FFrame`，不依赖 C++20 协程。

## 4. 语言 v1 规格

- **语句**：变量赋值、`if/else`、`while`、`repeat N`、函数定义/调用、`return`
- **表达式**：int/bool 字面量、标识符、算术 `+ - * / %`、比较 `== != < > <= >=`、布尔 `and/or/not`
- **机器人原生 API**（Sim 提供，每次调用消耗 tick 并挂起 VM）：
  `move(dir)`、`plant(crop)`、`harvest()`、`water()`、`till()`、`sense()`、`pos()`、`inventory(item)`、`can_harvest()`、`wait()`
- 类型 v1：仅 int 与 bool（字符串/浮点/数组留待 v2）。

## 5. 里程碑与验收

| 里程碑 | 内容 | 验收 |
|---|---|---|
| M0 | 规则+CLAUDE.md+git；CMake 骨架；SDL2+ImGui+imnodes 起窗 | 配置/构建通过，窗口出现且 ImGui/imnodes 可交互 |
| M1 | 语言与 VM 核心（无头） | `ctest` 黄金程序（含 while/函数/嵌套）输出正确，VM 可单步/暂停 |
| M2 | 农场仿真 + 原生 API | 无头黄金程序在固定种子世界达成产量目标 |
| M3 | 渲染 + 首个可玩 | 加载示例文本程序，可见机器人作业，调速正常 |
| M4 | 蓝图编辑器 | 蓝图拼出等价程序并能运行 |
| M5 | 双向同构同步 | 往返测试通过；改一端另一端实时同步 |
| M6 | 进程与内容 | 关卡/目标/解锁/更多作物指令 |

## 6. Commit 规范

格式：`type(scope): description`

```
type:  feat | fix | refactor | test | docs | chore | build
scope: lang | vm | blueprint | sim | game | infra
```

尾部加：
```
Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
```

> 仅在用户明确要求时提交/推送。
