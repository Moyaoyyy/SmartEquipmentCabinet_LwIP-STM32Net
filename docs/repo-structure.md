# 仓库结构与模块职责

## 目录总览
```text
.
├─ mcu/
│  ├─ app/
│  │  ├─ app_auth/
│  │  ├─ app_data/
│  │  ├─ app_lwip/
│  │  ├─ app_uplink/
│  │  ├─ task_lvgl/
│  │  ├─ task_rfid_auth/
│  │  └─ task_uplink/
│  ├─ bsp/
│  │  ├─ locker/
│  │  ├─ nfc/
│  │  ├─ lcd/
│  │  ├─ touch/
│  │  └─ eth/
│  ├─ middleware/
│  │  ├─ lvgl/
│  │  └─ lwip/
│  └─ user/
│     └─ main.c
├─ project/
├─ docs/
└─ README.md
```

## `mcu/app` 模块说明
- `app_auth`：同步鉴权客户端，构造并发送 `RFID_AUTH_REQ`，输出 `allow_open/network_fail/code`。
- `app_data`：跨任务共享会话数据，维护当前门位、会话状态、UI 动作位。
- `app_lwip`：网络初始化封装。
- `app_uplink`：异步上报引擎，包含队列、重试、JSON 编解码、HTTP 传输。
- `task_lvgl`：UI 状态机与触摸事件处理。
- `task_rfid_auth`：RFID 业务主状态机，负责读卡、鉴权、开门、会话流转、审计入队。
- `task_uplink`：异步发送调度任务，周期调用 `uplink_poll()`。

## 关键代码入口
- 启动与任务编排：`mcu/user/main.c`
- 同步鉴权：`mcu/app/app_auth/Src/app_auth.c`
- 异步发送核心：`mcu/app/app_uplink/Src/uplink.c`
- RFID 主流程：`mcu/app/task_rfid_auth/Src/task_rfid_auth.c`
- UI 流程：`mcu/app/task_lvgl/Src/task_lvgl.c`

## 模块协作关系
1. `Task_Lvgl` 负责用户输入和页面提示。
2. `Task_RfidAuth` 消费 UI 动作，处理读卡与鉴权。
3. 放行后调用 `Locker_Open()` 执行开门。
4. `Task_RfidAuth_Audit` 将审计消息入 `app_uplink` 队列。
5. `Task_Uplink` 轮询发送并按策略重试。

## 文档跳转
- [项目概览](overview.md)
- [系统架构](architecture.md)
- [用户行为流程](user-flow.md)
- [上传详细流程](uplink-flow.md)
- [新接手开发者指南](handover.md)
- [未来接入电磁门阀方案](door-actuator-integration.md)
