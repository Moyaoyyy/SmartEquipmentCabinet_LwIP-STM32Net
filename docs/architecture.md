# 系统架构

## 分层说明
- `bsp/*`：板级硬件驱动（LCD、触摸、ETH、RC522、门锁等）。
- `mcu/middleware/*`：中间件（LwIP、LVGL）。
- `mcu/app/app_*`：应用公共模块（共享数据、鉴权、上报、网络封装）。
- `mcu/app/task_*`：业务任务（UI、RFID、异步发送）。
- `mcu/user/main.c`：启动入口与任务编排。

## 架构关系图（ASCII）
```text
+-----------------------------------------------------------+
|                       Application Tasks                    |
|  Task_Lvgl         Task_RfidAuth          Task_Uplink      |
+--------------------------+----------------+----------------+
                           |                |
                           v                v
+-----------------------------------------------------------+
|                      App Modules                           |
|  app_data   app_auth   app_uplink   app_lwip              |
+--------------------------+----------------+----------------+
                           |                |
                           v                v
+-----------------------------------------------------------+
|                    Middleware Layer                        |
|                      LVGL / LwIP                           |
+--------------------------+----------------+----------------+
                           |                |
                           v                v
+-----------------------------------------------------------+
|                      BSP Drivers                           |
|  lcd touch eth nfc locker led usart ...                   |
+-----------------------------------------------------------+
```

## 启动时序（`main.c`）
1. `SystemClock_Config()` + `BSP_Init()`。
2. 创建 `AppTaskCreate`。
3. 在 `AppTaskCreate` 内按顺序初始化：
   - `LwIP_Init()`
   - `AppData_Init()`
   - `Task_Uplink_Init()`
   - `Task_Lvgl_Init()`
   - `Task_RfidAuth_Init()`
4. 创建任务：`Task_Uplink`、`Task_Lvgl`、`Task_RfidAuth`。

## 关键模块关系
- `Task_RfidAuth` 通过 `app_data` 与 `Task_Lvgl` 解耦协作。
- `Task_RfidAuth` 调用 `app_auth` 发起同步鉴权（实时决策是否开门）。
- `Task_RfidAuth` 调用 `app_uplink` 入队审计事件。
- `Task_Uplink` 周期调用 `uplink_poll()`，推进异步发送与重试。
