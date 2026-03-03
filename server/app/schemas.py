"""
文件作用：API 请求/响应数据模型定义。

主要职责：
- 用 Pydantic 描述 MCU 上报事件结构。
- 统一响应体结构，保证 `code/msg/traceId` 字段稳定。

依赖/调用关系：
- `router_uplink.py` 调用 `parse_uplink_event` 做请求数据校验。
"""

from typing import Any, Dict, Optional

from pydantic import BaseModel


class UplinkEvent(BaseModel):
    """
    用途：描述 MCU 上报事件外层结构。

    字段说明：
    - deviceId: 设备唯一标识。
    - messageId: 消息编号（用于幂等判重）。
    - ts: 设备侧时间戳（毫秒）。
    - type: 事件类型，如 `RFID_AUTH_REQ` / `RFID_AUDIT`。
    - payload: 业务载荷对象。
    """

    deviceId: str
    messageId: int
    ts: int
    type: str
    payload: Dict[str, Any]


class UplinkResponse(BaseModel):
    """
    用途：统一 API 返回结构。

    字段说明：
    - code: 业务码。
    - msg: 可读消息。
    - traceId: 服务端链路追踪 ID。
    """

    code: int
    msg: Optional[str] = None
    traceId: Optional[str] = None


def parse_uplink_event(payload: Dict[str, Any]) -> UplinkEvent:
    """
    用途：把原始字典解析为 `UplinkEvent` 对象。

    参数：
    - payload: 原始 JSON 字典。

    返回值：
    - UplinkEvent: 通过模型校验后的事件对象。

    异常：
    - 当字段缺失或类型不匹配时，Pydantic 会抛出校验异常。
    """
    return UplinkEvent.parse_obj(payload)
