"""
文件作用：同步鉴权业务处理模块。

主要职责：
- 接收 `RFID_AUTH_REQ` 的 payload。
- 执行业务判定（卡是否注册、是否有门位权限、是否重复请求）。
- 生成业务码并持久化鉴权决策日志。

依赖/调用关系：
- 由 `router_uplink.py` 调用。
- 使用 `repo_sqlite.SQLiteRepo` 读写设备权限与鉴权记录。
"""

from typing import Any, Dict, Tuple

from .repo_sqlite import SQLiteRepo


def _message_for_code(code: int) -> str:
    """
    用途：业务码映射为可读消息。

    参数：
    - code: 业务码。

    返回值：
    - str: 业务码对应文本。
    """
    mapping = {
        0: "ok",
        1001: "card_not_registered",
        1002: "no_locker_permission",
        1003: "locker_unavailable",
        1004: "duplicate_request",
        5001: "service_busy",
        5002: "system_maintenance",
    }
    return mapping.get(code, "unknown")


def handle_auth_event(
    repo: SQLiteRepo,
    trace_id: str,
    device_id: str,
    message_id: int,
    payload: Dict[str, Any],
) -> Tuple[int, str]:
    """
    用途：处理同步鉴权事件。

    参数：
    - repo: SQLite 仓储实例。
    - trace_id: 服务端追踪 ID。
    - device_id: 设备 ID。
    - message_id: 消息 ID（用于幂等判重）。
    - payload: 鉴权业务字段（lockerId/uid/uidSha1 等）。

    返回值：
    - Tuple[int, str]: `(业务码, 文本消息)`。

    边界行为：
    - payload 字段缺失时返回 `5001`。
    - 同一设备同一 messageId 重复提交时返回 `1004`。
    """
    locker_id = str(payload.get("lockerId", "")).strip()
    uid = str(payload.get("uid", "")).strip()
    uid_sha1 = str(payload.get("uidSha1", "")).strip().lower()

    # 必要字段校验，缺失时视为请求无效。
    if not locker_id or not uid or not uid_sha1:
        code = 5001
        return code, "invalid_auth_payload"

    # 幂等处理：同设备+同 messageId 视为重复请求。
    duplicate = repo.get_auth_decision(device_id, message_id)
    if duplicate is not None:
        return 1004, _message_for_code(1004)

    # 业务判定顺序：先确认卡存在，再确认门权限。
    if not repo.has_card(uid_sha1):
        code = 1001
    elif not repo.has_permission(uid_sha1, locker_id):
        code = 1002
    else:
        code = 0

    msg = _message_for_code(code)

    # 无论放行或拒绝，都记录一次鉴权决策，便于追踪。
    repo.insert_auth_decision(
        trace_id=trace_id,
        device_id=device_id,
        message_id=message_id,
        locker_id=locker_id,
        uid=uid,
        uid_sha1=uid_sha1,
        code=code,
        msg=msg,
    )

    return code, msg
