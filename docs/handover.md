﻿# 新接手开发者指南

本文给新接手同学一条最短上手路径，按顺序执行即可。

## 新接手先看这一段：同步与异步一页读懂
- 同步链路（`RFID_AUTH_REQ`）用于“是否开门”的实时决策。
- 异步链路（`RFID_AUDIT`）用于“全过程留痕”的可靠上报。
- 同步触发点：`Task_RfidAuth` 读卡后直接调用 `AppAuth_Verify`。
- 异步触发点：`Task_RfidAuth_Audit` 在关键节点把事件入队。
- 同步成功条件：`HTTP 2xx && code==0`，否则不放行。
- 异步成功条件：`HTTP 2xx && (code==0 或 code缺失)`，失败重试。
- 同步问题优先看：超时、HTTP 状态、`code` 解析。
- 异步问题优先看：是否入队、`Task_Uplink` 是否轮询、重试是否达上限。
- 线上排障顺序：先确认同步决策是否正确，再检查异步审计完整性。
- 详细实现与字段说明见 [上传详细流程](uplink-flow.md)。

## 第 1 步：先读这三份文档
1. `README.md`：了解当前能力边界。
2. `docs/user-flow.md`：理解用户实际交互。
3. `docs/uplink-flow.md`：理解“同步鉴权 + 异步审计”双链路。

## 第 2 步：确认代码主入口
- 启动入口：`mcu/user/main.c`
- 核心任务：`Task_Uplink`、`Task_Lvgl`、`Task_RfidAuth`
- 关键共享：`app_data`

建议先从 `main.c` 跳转到三个任务实现，建立调用链心智图。

## 第 3 步：本地构建与烧录
按 `docs/build-and-flash.md` 执行：
- Configure
- Build
- Flash

若构建失败，优先检查：
- include 路径是否跟随目录重命名更新。
- 是否仍引用旧符号（`Task_UplinkADC`、`Task_Light` 等）。

## 第 4 步：功能联调顺序
1. 先看 UI：门位选择、刷卡引导、错误提示是否正常。
2. 再看鉴权：抓取 `RFID_AUTH_REQ` 请求字段是否完整。
3. 最后看审计：`RFID_AUDIT` 是否入队、发送、重试正常。

## 第 5 步：修改前后检查清单
- 修改前：确认变更属于哪层（BSP / app / task）。
- 修改中：不改协议字段时，尽量不动 `app_auth` 和 `uplink_codec_json`。
- 修改后：至少回归以下场景：
  - 正常放行
  - 业务拒绝
  - 网络失败不放行
  - 开门后超时自动结束

## 如何更好的协作
- 先提交“现象 + 复现步骤 + 日志片段”。
- 再提交“定位结论 + 修复点”。
- 文档同步更新到对应 `docs/*.md`，不要把细节重新堆回根 README。
- 若链路行为有修改，必须同步更新 `docs/uplink-flow.md`，再更新本页摘要。
