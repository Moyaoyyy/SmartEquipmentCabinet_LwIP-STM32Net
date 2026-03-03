"""
文件作用：本机快速冒烟测试脚本。

主要职责：
- 发送一条同步鉴权请求（RFID_AUTH_REQ）。
- 发送一条异步审计请求（RFID_AUDIT）。
- 打印服务端返回，用于快速确认接口可用。

使用场景：
- 服务启动后执行，验证“路由 + 业务处理 + 数据库写入”链路。
"""

import json
from pathlib import Path
import sys
import urllib.request

# 让脚本可从 `server/tools` 直接执行并导入 `app` 包。
ROOT_DIR = Path(__file__).resolve().parents[1]
if str(ROOT_DIR) not in sys.path:
    sys.path.insert(0, str(ROOT_DIR))

URL = "http://127.0.0.1:8080/api/uplink"


def post(event: dict) -> dict:
    """
    用途：向上级服务发送一条 JSON 事件。

    参数：
    - event: 事件字典（包含 deviceId/messageId/type/payload）。

    返回值：
    - dict: 服务端 JSON 响应。
    """
    payload = json.dumps(event).encode("utf-8")
    req = urllib.request.Request(
        URL,
        data=payload,
        method="POST",
        headers={"Content-Type": "application/json"},
    )
    with urllib.request.urlopen(req, timeout=3) as resp:
        body = resp.read().decode("utf-8")
        return json.loads(body)


def main() -> None:
    """
    用途：执行同步+异步两条链路的冒烟请求。

    参数：
    - 无。

    返回值：
    - 无（结果打印到终端）。
    """
    auth_req = {
        "deviceId": "stm32f4",
        "messageId": 1,
        "ts": 1710000000000,
        "type": "RFID_AUTH_REQ",
        "payload": {
            "lockerId": "A01",
            "uid": "A1B2C3D4",
            "uidSha1": "1111111111111111111111111111111111111111",
            "deviceId": "stm32f4",
            "sessionId": 1,
            "clientTsMs": 1710000000000,
        },
    }

    audit_req = {
        "deviceId": "stm32f4",
        "messageId": 2,
        "ts": 1710000001000,
        "type": "RFID_AUDIT",
        "payload": {
            "ev": "CARD_READ",
            "sid": 1,
            "lockerId": "A01",
            "uid": "A1B2C3D4",
            "code": 0,
            "http": 200,
            "net": 1,
            "door": 1,
            "cache": 0,
            "drop": 0,
        },
    }

    # 先验证同步鉴权。
    print("AUTH =>", post(auth_req))
    # 再验证异步审计。
    print("AUDIT =>", post(audit_req))


if __name__ == "__main__":
    main()
