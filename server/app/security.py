"""
文件作用：设备签名校验与防重放模块。

主要职责：
- 在请求入口校验设备签名是否合法。
- 通过时间窗与 nonce 缓存降低重放攻击风险。
- 在签名通过后更新设备最近访问时间。

依赖/调用关系：
- 由 `router_uplink.py` 调用 `verify_signature`。
- 使用 `repo_sqlite.SQLiteRepo` 查询设备密钥。
"""

import hashlib
import hmac
import time
from dataclasses import dataclass, field
from typing import Dict, Tuple

from fastapi.datastructures import Headers

from .config import Settings
from .repo_sqlite import SQLiteRepo


@dataclass
class NonceStore:
    """
    用途：内存级 nonce 缓存，用于防重放。

    字段说明：
    - ttl_sec: nonce 有效期（秒）。
    - _nonces: `nonce_key -> 过期时间戳` 映射。
    """

    ttl_sec: int
    _nonces: Dict[str, int] = field(default_factory=dict)

    def seen(self, key: str, now_sec: int) -> bool:
        """
        用途：判断 nonce 是否重复出现。

        参数：
        - key: 唯一 nonce 键（建议包含 device_id）。
        - now_sec: 当前 Unix 时间戳（秒）。

        返回值：
        - bool:
          - True: 视为重放（已见过且未过期）。
          - False: 首次出现，已写入缓存。
        """
        self._cleanup(now_sec)
        expire_at = self._nonces.get(key)
        if expire_at is not None and expire_at >= now_sec:
            return True

        self._nonces[key] = now_sec + self.ttl_sec
        return False

    def _cleanup(self, now_sec: int) -> None:
        """
        用途：清理已过期 nonce，避免内存无限增长。

        参数：
        - now_sec: 当前 Unix 时间戳（秒）。

        返回值：
        - 无。
        """
        stale = [k for k, exp in self._nonces.items() if exp < now_sec]
        for k in stale:
            self._nonces.pop(k, None)


def verify_signature(
    headers: Headers,
    raw_body: bytes,
    repo: SQLiteRepo,
    settings: Settings,
    nonce_store: NonceStore,
) -> Tuple[bool, str]:
    """
    用途：校验请求签名。

    参数：
    - headers: HTTP 请求头对象。
    - raw_body: 请求原始字节串（参与签名计算）。
    - repo: SQLite 仓储实例。
    - settings: 配置对象（签名开关、时间窗、nonce TTL）。
    - nonce_store: nonce 缓存实例。

    返回值：
    - Tuple[bool, str]: `(是否通过, 说明消息)`。

    校验流程：
    1. 可选模式下，若未携带签名可直接放行。
    2. 校验签名相关请求头是否齐全。
    3. 校验时间戳格式与时间窗口。
    4. 校验 nonce 是否重复。
    5. 校验设备是否注册且启用。
    6. 计算 HMAC-SHA256 并常量时间比对。
    """
    signature = headers.get("X-Signature")

    # 联调阶段可关闭强制签名，便于快速接入。
    if not settings.signature_required and not signature:
        return True, "signature_optional"

    device_id = headers.get("X-Device-Id")
    ts = headers.get("X-Timestamp")
    nonce = headers.get("X-Nonce")

    if not device_id or not ts or not nonce or not signature:
        return False, "signature_header_missing"

    try:
        ts_int = int(ts)
    except ValueError:
        return False, "invalid_timestamp"

    now_sec = int(time.time())
    # 时间窗口检查，防止旧请求在较长时间后被重放。
    if abs(now_sec - ts_int) > settings.signature_max_skew_sec:
        return False, "timestamp_out_of_range"

    # nonce 检查，防止同时间窗口内重复提交。
    if nonce_store.seen(f"{device_id}:{nonce}", now_sec):
        return False, "nonce_replay"

    device = repo.get_device(device_id)
    if not device or int(device.get("status", 0)) != 1:
        return False, "device_not_registered"

    secret = device.get("secret", "")

    # 签名原文格式：timestamp + '\n' + nonce + '\n' + body
    data_to_sign = f"{ts}\n{nonce}\n".encode("utf-8") + raw_body
    expected = hmac.new(secret.encode("utf-8"), data_to_sign, hashlib.sha256).hexdigest()

    # 使用 compare_digest 避免常见时序攻击。
    if not hmac.compare_digest(expected.lower(), signature.lower()):
        return False, "signature_mismatch"

    repo.touch_device_seen(device_id)
    return True, "ok"
