"""
文件作用：异步审计业务处理模块。

主要职责：
- 接收 `RFID_AUDIT` 审计 payload。
- 做最小字段校验。
- 将事件写入审计表。

依赖/调用关系：
- 由 `router_uplink.py` 调用。
- 使用 `repo_sqlite.SQLiteRepo` 写入数据库。
"""

from typing import Any, Dict, Tuple

from .repo_sqlite import SQLiteRepo


# 当前审计事件最小必填字段。
_REQUIRED_FIELDS = ("ev", "sid", "lockerId", "uid")


def handle_audit_event(
    repo: SQLiteRepo,
    trace_id: str,
    device_id: str,
    message_id: int,
    payload: Dict[str, Any],
) -> Tuple[int, str]:
    """
    用途：处理异步审计事件并入库。

    参数：
    - repo: SQLite 仓储实例。
    - trace_id: 服务端追踪 ID。
    - device_id: 设备 ID。
    - message_id: 消息 ID。
    - payload: 审计字段对象。

    返回值：
    - Tuple[int, str]: `(业务码, 文本消息)`。

    边界行为：
    - 缺少关键字段时返回 `5001`。
    """
    for key in _REQUIRED_FIELDS:
        if key not in payload:
            return 5001, f"invalid_audit_payload_missing_{key}"

    # 审计数据直接入库，便于后续追溯。
    repo.insert_audit_event(
        trace_id=trace_id,
        device_id=device_id,
        message_id=message_id,
        payload=payload,
    )
    return 0, "ok"
