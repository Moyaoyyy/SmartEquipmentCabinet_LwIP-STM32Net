# 上级服务（FastAPI）

本目录用于替代旧的 `test_server.py` 测试脚本，提供可维护的正式上级服务实现。
服务兼容 MCU 现有协议，统一接收 `/api/uplink` 事件并按类型分发处理。

## 功能概览
- 同步鉴权：处理 `RFID_AUTH_REQ`，返回 `code` 决策是否放行。
- 异步审计：处理 `RFID_AUDIT`，落库保存过程事件。
- 数据落盘：使用 Python 内置 `sqlite3`，无需单独安装 SQLite 客户端。
- 安全预留：支持设备签名校验开关（联调可关闭，部署可开启）。

## 快速启动（本机）
在仓库根目录执行：

```powershell
cd server
.\run_local.ps1
```

启动后默认监听：`0.0.0.0:8080`。

## 配置项说明（`.env`）
首次运行会自动从 `.env.example` 复制出 `.env`。

常用配置：
- `APP_HOST`：监听地址，默认 `0.0.0.0`
- `APP_PORT`：监听端口，默认 `8080`
- `DB_PATH`：数据库路径，默认 `./data/uplink.db`
- `DATA_RETENTION_DAYS`：审计保留天数，默认 `30`
- `SIGNATURE_REQUIRED`：是否强制签名，`0/1`
- `SIGNATURE_MAX_SKEW_SEC`：签名时间戳允许偏差秒数
- `NONCE_TTL_SEC`：防重放 nonce 保留秒数
- `LOG_LEVEL`：日志级别（`INFO/DEBUG`）

## API 说明
### 1) 上报入口
- `POST /api/uplink`

请求外层结构：

```json
{
  "deviceId": "stm32f4",
  "messageId": 1,
  "ts": 1710000000000,
  "type": "RFID_AUTH_REQ",
  "payload": {}
}
```

响应结构：

```json
{
  "code": 0,
  "msg": "ok",
  "traceId": "xxxxxxxx"
}
```

### 2) 健康检查
- `GET /healthz`

## SQLite 说明
- 本服务直接使用 Python 标准库 `sqlite3`。
- 数据库默认文件：`server/data/uplink.db`。
- 表结构由服务启动自动初始化。

## 本机联调流程（seed + smoke）
先确保服务已启动，再开新终端执行：

```powershell
cd server
.\.venv\Scripts\python.exe tools\seed_demo_data.py
.\.venv\Scripts\python.exe tools\smoke_test.py
```

说明：
- `seed_demo_data.py`：写入演示设备与权限。
- `smoke_test.py`：发送一条鉴权请求和一条审计请求。

## 迁移到 RK3568（阶段B）
1. 将 `server/` 拷贝到 RK3568（例如 `/opt/rfid/server`）。
2. 创建虚拟环境并安装依赖。
3. 按内网地址修改 `.env`。
4. 配置 systemd 开机自启。

示例 systemd：

```ini
[Unit]
Description=RFID Uplink Server
After=network.target

[Service]
WorkingDirectory=/opt/rfid/server
ExecStart=/opt/rfid/server/.venv/bin/python -m uvicorn app.main:app --host 0.0.0.0 --port 8080
Restart=always
RestartSec=2
User=root

[Install]
WantedBy=multi-user.target
```

## 常见问题
1. 启动时报端口占用：
- 修改 `.env` 的 `APP_PORT`，或释放占用进程。

2. MCU 无法连通服务：
- 确认 MCU 配置的上级 IP 是电脑/RK3568 的局域网 IP，不是 `127.0.0.1`。

3. 审计数据未保留：
- 检查 `DB_PATH` 是否落到预期目录，确认服务进程有写权限。

4. 签名校验失败：
- 联调阶段可设 `SIGNATURE_REQUIRED=0`；上线阶段再切 `1` 并同步设备签名逻辑。
