# 项目概览

## 项目目标
实现智能器材柜“刷卡鉴权后开门”的完整闭环：
- 用户选门并刷校园卡
- 设备同步请求上级判定
- 放行后开门并引导用户完成取物
- 关键节点异步审计上报

## 软硬件基础
- MCU：STM32F429IGTx
- RTOS：FreeRTOS
- 网络：LwIP（NO_SYS=0）
- GUI：LVGL
- RFID：RC522
- 门锁执行：`bsp_locker`（当前为 STUB，可平滑替换真实驱动）

## 当前主链路
1. 系统启动后初始化网络、共享数据、上传模块、UI、RFID 鉴权模块。
2. UI 进入门位选择与刷卡引导。
3. RFID 任务读取 UID 并调用同步鉴权接口。
4. 鉴权通过后执行开门；失败或网络异常则拒绝开门。
5. 审计事件通过异步队列上传。

## 能力边界
- 当前仅使用 UID 做鉴权，不读取卡扇区业务数据。
- 当前无门磁传感器，取物完成由触摸按钮确认。
- 网络异常时不放行。

## 相关文档
- [系统架构](architecture.md)
- [用户行为流程](user-flow.md)
- [上传详细流程](uplink-flow.md)
- [构建与烧录](build-and-flash.md)
- [仓库结构与模块职责](repo-structure.md)
- [新接手开发者指南](handover.md)
- [未来接入电磁门阀方案](door-actuator-integration.md)
