# 参考代码与游戏规则

## 0. 硬约束（最高优先级）— UE5 仓库只读

**`C:\Users\admin\Documents\UE5NEWAI` 全树只读。**

- 仅允许 Read / Grep / Glob 查阅，用于设计对照。
- **禁止任何写、改、删、移动、格式化、git 操作**，包括其 `.claude/`、`rules/`、`UE5engine/` 等所有子目录。
- 所有代码改动只发生在 `E:\SystemGAMEProject`。
- 不复制 UE5 的 C++ 源码进本项目（许可不同）；只参考其架构与设计范式。

## 1. UE5 源码参考映射

根：`C:\Users\admin\Documents\UE5NEWAI\UE5engine\Engine\Source\`

| 我们要做的 | UE5 对应路径 |
|---|---|
| 字节码 VM / 指令解释循环 | `Runtime\CoreUObject\Private\UObject\ScriptCore.cpp`（`UObject::ProcessInternal` / `FFrame`） |
| 操作码定义 | `Runtime\CoreUObject\Public\UObject\Script.h`（`EExprToken`） |
| VM 栈帧 / 执行上下文 | `Runtime\CoreUObject\Public\UObject\Stack.h`、`ScriptMacros.h` |
| 蓝图节点图 → 字节码编译器 | `Editor\KismetCompiler\` |
| 节点图数据模型（K2 节点 / Pin / 连线） | `Editor\BlueprintGraph\` |
| 蓝图编辑器交互范式（只看交互思路） | `Editor\Kismet\`、`Editor\GraphEditor\`、`Editor\KismetWidgets\` |
| 协程式逐帧恢复执行（对标逐 tick 挂起） | `Runtime\Engine\Classes\Engine\LatentActionManager.h` |

## 2. 参考原则

- 只参考**架构 / 设计范式 / 数据模型组织方式**，不抄代码。
- 每次借鉴必须在本文件"借鉴记录"追加一条：借鉴了什么设计 → 落到本项目哪个文件 → 与 UE5 的差异。

## 3. 参考游戏

*The Farmer Was Replaced* —— 玩法基线：
- 机器人执行玩家程序，永续自动化循环。
- 世界随 tick 持续推进（作物在程序运行时同步生长）。
- 机器人 API 为指令式、每动作消耗时间。

## 4. 查阅纪律

修改 VM / 编译器 / 蓝图编解码前，先 Read 对应 UE5 文件做设计对照，把结论回写到下方"借鉴记录"。

## 5. 借鉴记录

> 格式：`[日期] 借鉴点 → 本项目落点 → 差异`

- [2026-05-18] **UE5 `FFrame`（Stack.h）的显式执行帧模型** → 落点 `src/vm/vm.{hpp,cpp}` 的 `CallFrame`/`VM`。
  借鉴：`FFrame` 用 `uint8* Code`(PC) + `Locals` + `FlowStack`(跳转) + `FFrame* PreviousFrame`(显式调用链，非 C++ 递归)，由外部 `Step()` 驱动一条指令 → 因此天然可暂停/恢复/单步/序列化。
  本项目落点：VM 用显式 `CallFrame` 栈 + 共享操作数栈 + 指令索引 PC，`run(budget)` 外部驱动；不使用 C++ 调用栈做语言级函数调用。
  差异：我们用栈式字节码（非 UE 的属性/Code 字节流），变量按名解析（globals/locals map）保证与树解释器语义逐一对齐；`FlowStack` 用编译期跳转目标替代。
- [2026-05-18] **UE5 Latent（LatentActionManager.h）跨帧恢复** → 落点：M2 的逐 tick 原生挂起设计依据。
  借鉴：latent 节点登记回调 + linkage，动作完成后在后续 tick 于正确字节码偏移恢复。
  本项目落点（M2）：机器人原生动作消耗 tick → VM 返回挂起态，游戏 tick 完成后 `run()` 续跑。M1.4 先用 `run(step budget)` 协作式暂停证明可恢复性，不依赖 sim。
