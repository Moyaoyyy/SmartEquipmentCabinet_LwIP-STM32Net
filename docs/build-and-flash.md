# 构建与烧录

## VSCode 任务映射
推荐使用 `.vscode/tasks.json` 中的任务。

### 1) 配置（CMake Configure）
```bash
cmake -DCMAKE_TOOLCHAIN_FILE=${cwd}/project/arm-gnu-none-eabi.cmake \
      -DCMAKE_SYSTEM_NAME=Generic \
      -DCMAKE_EXPORT_COMPILE_COMMANDS:BOOL=TRUE \
      -GNinja \
      -S project \
      -B build
```

### 2) 编译（CMake Build）
```bash
cmake --build ${cwd}/build --target all
```

### 3) 烧录（Flash）
```bash
openocd -f interface/cmsis-dap.cfg \
        -f target/stm32f4x.cfg \
        -c "transport select swd" \
        -c "program {${workspaceFolder}/build/template.elf} verify reset exit"
```

## 常见问题
- 目录重命名后 IntelliSense 仍报 include 错误：
  - 检查 `.vscode/c_cpp_properties.json` 的 `includePath` 是否同步更新。
- `build` 目录状态异常：
  - 先执行 `Clean`，再重新 `Configure` 和 `Build`。

## 建议验收项
- 生成 `build/template.elf`。
- 无旧符号残留：`Task_UplinkADC`、`task_uplink_adc`、`Task_Light`。
