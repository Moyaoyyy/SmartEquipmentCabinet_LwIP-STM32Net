# 智能实验室器材柜（STM32F429 + LwIP + RFID）

本项目是智能器材柜嵌入式固件，当前业务主链路为“选门 -> 刷卡 -> 在线鉴权 -> 开门 -> 会话回执”，并保留异步审计上报。

## 当前能力
- RFID 主流程：门位选择、刷卡、同步鉴权、开门、完成确认。
- UI 引导：基于 LVGL 状态机页面，覆盖等待刷卡、鉴权中、通过、拒绝、网络异常等状态。
- 数据上传：
  - 同步鉴权请求 `RFID_AUTH_REQ`
  - 异步审计事件 `RFID_AUDIT`
- 网络与系统：LwIP + FreeRTOS（`NO_SYS=0`）。

## 快速开始
- 配置（对应 VSCode 任务 `CMake Configure`）：
  - `cmake -DCMAKE_TOOLCHAIN_FILE=${cwd}/project/arm-gnu-none-eabi.cmake -DCMAKE_SYSTEM_NAME=Generic -DCMAKE_EXPORT_COMPILE_COMMANDS:BOOL=TRUE -GNinja -S project -B build`
- 构建（对应 VSCode 任务 `CMake Build`）：
  - `cmake --build ${cwd}/build --target all`
- 烧录（对应 VSCode 任务 `Flash`）：
  - `openocd -f interface/cmsis-dap.cfg -f target/stm32f4x.cfg -c "transport select swd" -c "program {build/template.elf} verify reset exit"`

## 文档索引
- [项目概览](docs/overview.md)
- [系统架构](docs/architecture.md)
- [用户行为流程](docs/user-flow.md)
- [上传详细流程](docs/uplink-flow.md)
- [构建与烧录](docs/build-and-flash.md)
- [仓库结构与模块职责](docs/repo-structure.md)
- [协同开发指南](docs/handover.md)
- [未来接入电磁门阀方案](docs/door-actuator-integration.md)

## 版本与状态
- 当前任务命名为 `Task_Uplink`，历史命名 `Task_UplinkADC` 已下线。
- 旧光照测试链路（`Task_Light` / `LIGHT_ADC`）已下线。
- 当前 `mcu/app` 生效模块：`app_auth`、`app_data`、`app_lwip`、`app_uplink`、`task_lvgl`、`task_rfid_auth`、`task_uplink`。
