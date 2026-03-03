"""
文件作用：上级服务统一入口路由。

主要职责：
- 提供 `/api/uplink` 接口。
- 完成签名校验、请求解析、按 `type` 分发。
- 统一构造响应格式 `code/msg/traceId`。

依赖/调用关系：
- 调用 `security.verify_signature` 进行设备签名校验。
- 调用 `service_auth.handle_auth_event` 处理同步鉴权。
- 调用 `service_audit.handle_audit_event` 处理异步审计。
"""

import json
import logging
import uuid
from typing import Any, Dict

from fastapi import APIRouter, Request
from fastapi.responses import JSONResponse

from .schemas import UplinkResponse, parse_uplink_event
from .security import verify_signature
from .service_audit import handle_audit_event
from .service_auth import handle_auth_event


router = APIRouter()
logger = logging.getLogger("uplink.router")


def _json_response(code: int, msg: str, trace_id: str) -> JSONResponse:
    """
    用途：统一封装 API 返回体，避免各分支重复拼装。

    参数：
    - code: 业务码。
    - msg: 业务描述。
    - trace_id: 服务端追踪 ID。

    返回值：
    - JSONResponse: HTTP 200 + 统一 JSON 结构。
    """
    body = UplinkResponse(code=code, msg=msg, traceId=trace_id).dict(exclude_none=True)
    return JSONResponse(status_code=200, content=body)


@router.post("/api/uplink")
async def uplink_entry(request: Request) -> JSONResponse:
    """
    用途：上报入口处理函数。

    参数：
    - request: FastAPI 请求对象。

    返回值：
    - JSONResponse: 固定 HTTP 200，业务结果通过 `code` 表达。

    处理步骤：
    1. 生成 traceId。
    2. 校验签名（按配置可选/强制）。
    3. 解析 JSON 与事件模型。
    4. 按 `type` 分发到鉴权或审计处理。
    """
    # 为每次请求生成追踪 ID，便于日志与数据库记录关联。
    trace_id = uuid.uuid4().hex

    settings = request.app.state.settings
    repo = request.app.state.repo
    nonce_store = request.app.state.nonce_store

    # 原始 body 用于签名校验，也用于后续 JSON 解析。
    raw = await request.body()

    # 先做签名校验，失败时直接返回，不进入业务层。
    ok, sign_msg = verify_signature(request.headers, raw, repo, settings, nonce_store)
    if not ok:
        logger.warning("signature check failed trace=%s reason=%s", trace_id, sign_msg)
        return _json_response(5001, sign_msg, trace_id)

    try:
        event_dict: Dict[str, Any] = json.loads(raw.decode("utf-8"))
    except Exception:
        return _json_response(5001, "invalid_json", trace_id)

    try:
        event = parse_uplink_event(event_dict)
    except Exception:
        return _json_response(5001, "invalid_event_schema", trace_id)

    # 同步鉴权链路：返回结果直接影响 MCU 是否开门。
    if event.type == "RFID_AUTH_REQ":
        code, msg = handle_auth_event(
            repo=repo,
            trace_id=trace_id,
            device_id=event.deviceId,
            message_id=event.messageId,
            payload=event.payload,
        )
        return _json_response(code, msg, trace_id)

    # 异步审计链路：记录关键事件，主逻辑返回成功/失败码。
    if event.type == "RFID_AUDIT":
        code, msg = handle_audit_event(
            repo=repo,
            trace_id=trace_id,
            device_id=event.deviceId,
            message_id=event.messageId,
            payload=event.payload,
        )
        return _json_response(code, msg, trace_id)

    # 未支持类型统一返回维护类错误码。
    return _json_response(5002, f"unsupported_type_{event.type}", trace_id)
