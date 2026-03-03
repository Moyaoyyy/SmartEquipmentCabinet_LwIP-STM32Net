﻿# 未来接入电磁门阀方案

当前 `bsp_locker` 为 STUB 实现（`LED_PURPLE` 模拟开门脉冲窗口）。本文描述后续接入真实电磁门阀/电磁锁的推荐落地方式。

## 当前基线
- 接口已固定：
  - `Locker_Init()`
  - `Locker_Open(locker_index, pulse_ms)`
  - `Locker_GetId()` / `Locker_GetCount()`
- `Task_RfidAuth` 已在放行分支调用 `Locker_Open()`。
- STUB 可视化行为：开门脉冲期间显示 `LED_PURPLE`，脉冲结束后 `LED_RGBOFF`。
- 失败路径已具备：开门失败会进入拒绝态并审计 `DOOR_OPEN_FAIL`。

## 硬件接入建议
1. 每门一路控制输出（GPIO + MOSFET/继电器驱动）。
2. 电磁阀电源与 MCU 侧隔离，保留反灌/浪涌保护。
3. 建议预留门磁输入，后续支持“门状态闭环”。

## 软件改造步骤
1. 在 `bsp_locker.c` 的 `LOCKER_USE_STUB==0` 分支实现真实 IO：
   - `Locker_Init()`：初始化控制 GPIO、默认关断。
   - `Locker_Open()`：按 `pulse_ms` 输出开门脉冲。
2. 增加硬件错误码映射（如短路、过流、驱动故障），统一返回 `LOCKER_ERR_HW` 子类或扩展枚举。
3. 维持 `Task_RfidAuth` 调用点不变，确保上层业务逻辑无需改动。

## 若引入门磁（建议二期）
- 新增 `Locker_GetDoorState(locker_index)`。
- `Task_RfidAuth` 在开门后轮询门磁：
  - 观察到“已开”再提示用户取物。
  - 观察到“已关”可自动结束会话，减少手动确认依赖。
- 审计新增字段：`doorStateOpenTs`、`doorStateCloseTs`。

## 安全与策略建议
- 断网不放行策略保持不变。
- 开门脉冲设置上限，防止线圈过热。
- 若硬件开门失败，必须同时：
  - UI 给出明确告警
  - 记录并上报失败审计

## 回归测试建议
- 单门正常开门和超时关断。
- 多门连续触发，确认门位映射无串门。
- 开门失败分支是否正确提示与上报。
- 长时间运行下，线圈温升和任务调度是否稳定。
