# 上传详细流程

当前上传分为两条链路：
- 同步鉴权链路（决定是否开门）
- 异步审计链路（记录全过程）

## 一、代码实现定位（先看这一节）

### 同步鉴权主调用链
`Task_RfidAuth -> AppAuth_Verify -> transport.post_json`

- 触发入口：`mcu/app/task_rfid_auth/Src/task_rfid_auth.c`
- 同步鉴权实现：`mcu/app/app_auth/Src/app_auth.c`
- HTTP 发送实现：`mcu/app/app_uplink/Src/uplink_transport_http_netconn.c`

### 异步审计主调用链
`Task_RfidAuth_Audit -> uplink_enqueue_json -> Task_Uplink -> uplink_poll`

- 审计入队：`mcu/app/task_rfid_auth/Src/task_rfid_auth.c`
- 入队接口与发送状态机：`mcu/app/app_uplink/Src/uplink.c`
- 轮询调度任务：`mcu/app/task_uplink/Src/task_uplink.c`

## 二、同步鉴权链路（`RFID_AUTH_REQ`）

### 1. 触发点
`Task_RfidAuth` 在读到 UID 后调用 `AppAuth_Verify()`。

### 2. 请求构造
`AppAuth_Verify()` 先构造业务 payload：
- `lockerId`
- `uid`
- `uidSha1`
- `deviceId`
- `sessionId`
- `clientTsMs`

然后通过 `uplink_codec_json_build_event()` 包装成统一事件外壳，`type="RFID_AUTH_REQ"`。

### 3. 发送方式
- 直接调用 transport 的 `post_json()` 发送。
- 不进入异步队列。
- 默认超时：`send=1500ms`，`recv=1500ms`。

### 4. 响应判定
`AppAuth_Verify()` 判定规则：
- `allow_open=1`：`HTTP 2xx` 且业务 `code==0`。
- 业务拒绝：`HTTP 2xx` 且 `code!=0`。
- 网络失败：传输失败、非 2xx、响应 `code` 解析失败或缺失。

说明：网络失败统一标记 `network_fail=1`，主链路不放行。

## 三、异步审计链路（`RFID_AUDIT`）

### 1. 触发点
`Task_RfidAuth_Audit()` 在关键节点上报审计事件，典型 `ev` 包括：
- `CARD_READ`
- `AUTH_DENY`
- `AUTH_NET_FAIL`
- `DOOR_OPEN_OK`
- `DOOR_OPEN_FAIL`
- `SESSION_DONE`
- `SESSION_TIMEOUT`

### 2. 审计载荷
`payload` 字段：
- `ev`
- `sid`
- `lockerId`
- `uid`
- `code`
- `http`
- `net`
- `door`
- `cache`
- `drop`

### 3. 入队与限流
- 调用 `uplink_enqueue_json(&g_uplink, "RFID_AUDIT", payload)` 入队。
- 当队列深度接近满（`>= UPLINK_QUEUE_MAX_LEN - 1`）时，当前审计事件直接丢弃并累计 `drop`。
- 审计丢弃不阻塞主业务。

## 四、异步发送状态机（`uplink_poll`）

`Task_Uplink` 每 `100ms` 调用一次 `uplink_poll()`，每次最多处理队头 1 条消息。

### 1. 队头取出与可发送判定
- 上锁读取队头。
- 若未到 `next_retry_ms`，本轮返回。
- 若 `attempt` 超过策略上限（默认最大 10 次），直接丢弃队头。

### 2. 编码与发送
- 将 `type + payload` 编码为统一事件 JSON。
- 通过 `transport.post_json()` 发送 HTTP POST。

### 3. 成功与失败判定（异步链路）
`uplink_poll()` 成功条件：
- `HTTP 2xx`，且业务 `code==0` 或 `code` 缺失。

失败则计算下一次重试时间并保留队头。

### 4. 重试退避策略
默认策略来自 `uplink_config_set_defaults()`：
- `base_delay_ms = 500`
- `max_delay_ms = 10000`
- `max_attempts = 10`
- `jitter_pct = 20`

即指数退避 + 抖动，避免多设备同时重试造成拥塞。

## 五、为什么拆成“同步+异步”

- 安全性：开门是实时安全决策，必须同步拿到上级判定，不能先开门再补报。
- 体验与吞吐：审计不参与开门决策，异步队列可重试，不拖慢刷卡交互。
- 解耦：鉴权失败与审计失败可以独立处理，降低耦合和联动故障风险。
- 扩展性：后续升级 HTTPS 或更换传输层时，业务层流程无需重写。

## 六、成功/失败判定差异（同步 vs 异步）

| 维度 | 同步鉴权 `RFID_AUTH_REQ` | 异步审计 `RFID_AUDIT` |
| --- | --- | --- |
| 目的 | 决策是否开门 | 记录与追溯 |
| 成功判定 | `HTTP 2xx && code==0` | `HTTP 2xx && (code==0 或 code缺失)` |
| 失败后行为 | 直接不放行，进入拒绝/网络异常态 | 留在队列按退避重试，超限后丢弃 |
| 对用户影响 | 直接影响是否开门 | 不影响当次开门结果 |

## 七、常见问题与排查顺序

### 场景 1：刷卡后等待久
1. 先看同步链路超时：`AppAuth_Verify` 的 send/recv timeout。
2. 再看上级接口响应时延和 `HTTP/code` 返回。
3. 最后看 UI 状态机是否停在 `AUTH_PENDING` 未转移。

### 场景 2：开门正常但后台日志缺失
1. 检查 `Task_RfidAuth_Audit` 是否成功入队（队列是否接近满）。
2. 检查 `Task_Uplink` 是否按 `100ms` 周期调用 `uplink_poll`。
3. 检查重试是否已达上限导致丢弃。

### 场景 3：网络波动时误判
1. 先核对同步判定条件：是否被错误放宽（必须 `2xx && code==0`）。
2. 再核对异步判定条件：`code` 缺失是否按设计视为“审计发送成功”。
3. 对照抓包确认 HTTP 状态码和 body 结构。

## 八、链路总览
```text
Task_RfidAuth
  ├─ AppAuth_Verify (同步, 直接POST, 立即决策开门)
  └─ Task_RfidAuth_Audit -> uplink_enqueue_json (异步入队)

Task_Uplink (100ms)
  └─ uplink_poll -> 队头发送/失败退避/成功出队
```

