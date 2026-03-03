"""
文件作用：SQLite 数据访问层（Repository）。

主要职责：
- 初始化数据库与表结构。
- 提供设备、权限、鉴权决策、审计事件的读写接口。
- 提供按保留策略清理历史审计数据的接口。

依赖/调用关系：
- `main.py` 启动时调用 `init_db()`。
- `service_auth.py` 使用权限与决策相关接口。
- `service_audit.py` 使用审计入库接口。
- `security.py` 使用设备查询与心跳更新时间接口。
"""

import sqlite3
from contextlib import contextmanager
from datetime import datetime, timedelta, timezone
from pathlib import Path
from typing import Any, Dict, Optional


class SQLiteRepo:
    """
    用途：封装 SQLite 访问细节，向业务层提供稳定方法。

    参数：
    - db_path: SQLite 文件路径。
    """

    def __init__(self, db_path: Path) -> None:
        self._db_path = db_path

    @contextmanager
    def _conn(self):
        """
        用途：创建并管理数据库连接上下文。

        参数：
        - 无。

        返回值：
        - 迭代器上下文：yield `sqlite3.Connection`。

        关键行为：
        - 启用 WAL 模式以提升读写并发能力。
        - 退出时自动提交事务并关闭连接。
        """
        conn = sqlite3.connect(str(self._db_path), timeout=5.0)
        conn.row_factory = sqlite3.Row
        try:
            # WAL 模式对“读多写少”的审计场景更友好。
            conn.execute("PRAGMA journal_mode=WAL;")
            conn.execute("PRAGMA synchronous=NORMAL;")
            yield conn
            conn.commit()
        finally:
            conn.close()

    def init_db(self) -> None:
        """
        用途：初始化数据库目录与表结构。

        参数：
        - 无。

        返回值：
        - 无。

        说明：
        - 幂等执行，已存在的表/索引不会重复创建。
        """
        self._db_path.parent.mkdir(parents=True, exist_ok=True)
        with self._conn() as conn:
            conn.executescript(
                """
                CREATE TABLE IF NOT EXISTS devices (
                    device_id TEXT PRIMARY KEY,
                    secret TEXT NOT NULL,
                    status INTEGER NOT NULL DEFAULT 1,
                    last_seen_at TEXT
                );

                CREATE TABLE IF NOT EXISTS card_permissions (
                    uid_sha1 TEXT NOT NULL,
                    locker_id TEXT NOT NULL,
                    active INTEGER NOT NULL DEFAULT 1,
                    valid_from TEXT,
                    valid_to TEXT,
                    PRIMARY KEY(uid_sha1, locker_id)
                );

                CREATE TABLE IF NOT EXISTS auth_decisions (
                    id INTEGER PRIMARY KEY AUTOINCREMENT,
                    trace_id TEXT NOT NULL,
                    device_id TEXT NOT NULL,
                    message_id INTEGER NOT NULL,
                    locker_id TEXT NOT NULL,
                    uid TEXT NOT NULL,
                    uid_sha1 TEXT NOT NULL,
                    code INTEGER NOT NULL,
                    msg TEXT,
                    created_at TEXT NOT NULL,
                    UNIQUE(device_id, message_id)
                );

                CREATE TABLE IF NOT EXISTS audit_events (
                    id INTEGER PRIMARY KEY AUTOINCREMENT,
                    trace_id TEXT NOT NULL,
                    device_id TEXT NOT NULL,
                    message_id INTEGER NOT NULL,
                    ev TEXT NOT NULL,
                    sid INTEGER,
                    locker_id TEXT,
                    uid TEXT,
                    code INTEGER,
                    http INTEGER,
                    net INTEGER,
                    door INTEGER,
                    cache INTEGER,
                    drop INTEGER,
                    created_at TEXT NOT NULL
                );

                CREATE INDEX IF NOT EXISTS idx_audit_created_at ON audit_events(created_at);
                CREATE INDEX IF NOT EXISTS idx_auth_created_at ON auth_decisions(created_at);
                """
            )

    def get_device(self, device_id: str) -> Optional[Dict[str, Any]]:
        """
        用途：查询设备基础信息与密钥。

        参数：
        - device_id: 设备 ID。

        返回值：
        - dict | None: 命中返回设备记录，未命中返回 None。
        """
        with self._conn() as conn:
            row = conn.execute(
                "SELECT device_id, secret, status FROM devices WHERE device_id = ?",
                (device_id,),
            ).fetchone()
            return dict(row) if row else None

    def touch_device_seen(self, device_id: str) -> None:
        """
        用途：更新设备最近在线时间。

        参数：
        - device_id: 设备 ID。

        返回值：
        - 无。
        """
        with self._conn() as conn:
            conn.execute(
                "UPDATE devices SET last_seen_at = ? WHERE device_id = ?",
                (self._now_iso(), device_id),
            )

    def get_auth_decision(self, device_id: str, message_id: int) -> Optional[Dict[str, Any]]:
        """
        用途：查询某条消息是否已有鉴权结论（幂等判重）。

        参数：
        - device_id: 设备 ID。
        - message_id: 消息 ID。

        返回值：
        - dict | None: 命中返回决策记录，未命中返回 None。
        """
        with self._conn() as conn:
            row = conn.execute(
                "SELECT code, msg FROM auth_decisions WHERE device_id = ? AND message_id = ?",
                (device_id, message_id),
            ).fetchone()
            return dict(row) if row else None

    def insert_auth_decision(
        self,
        trace_id: str,
        device_id: str,
        message_id: int,
        locker_id: str,
        uid: str,
        uid_sha1: str,
        code: int,
        msg: str,
    ) -> None:
        """
        用途：写入鉴权决策日志。

        参数：
        - trace_id: 请求追踪 ID。
        - device_id: 设备 ID。
        - message_id: 消息 ID。
        - locker_id: 门位 ID。
        - uid: UID 原文。
        - uid_sha1: UID SHA1。
        - code: 业务码。
        - msg: 文本消息。

        返回值：
        - 无。

        说明：
        - 使用 `INSERT OR IGNORE`，配合唯一键实现幂等写入。
        """
        with self._conn() as conn:
            conn.execute(
                """
                INSERT OR IGNORE INTO auth_decisions
                (trace_id, device_id, message_id, locker_id, uid, uid_sha1, code, msg, created_at)
                VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?)
                """,
                (
                    trace_id,
                    device_id,
                    message_id,
                    locker_id,
                    uid,
                    uid_sha1,
                    code,
                    msg,
                    self._now_iso(),
                ),
            )

    def has_card(self, uid_sha1: str) -> bool:
        """
        用途：判断卡是否在权限系统中存在。

        参数：
        - uid_sha1: UID SHA1。

        返回值：
        - bool: True 表示存在，False 表示不存在。
        """
        with self._conn() as conn:
            row = conn.execute(
                "SELECT 1 FROM card_permissions WHERE uid_sha1 = ? LIMIT 1",
                (uid_sha1,),
            ).fetchone()
            return row is not None

    def has_permission(self, uid_sha1: str, locker_id: str) -> bool:
        """
        用途：判断卡在当前时间是否拥有指定门位权限。

        参数：
        - uid_sha1: UID SHA1。
        - locker_id: 门位 ID。

        返回值：
        - bool: True 表示有权限，False 表示无权限。
        """
        now = self._now_iso()
        with self._conn() as conn:
            row = conn.execute(
                """
                SELECT 1
                FROM card_permissions
                WHERE uid_sha1 = ?
                  AND locker_id = ?
                  AND active = 1
                  AND (valid_from IS NULL OR valid_from <= ?)
                  AND (valid_to IS NULL OR valid_to >= ?)
                LIMIT 1
                """,
                (uid_sha1, locker_id, now, now),
            ).fetchone()
            return row is not None

    def insert_audit_event(
        self,
        trace_id: str,
        device_id: str,
        message_id: int,
        payload: Dict[str, Any],
    ) -> None:
        """
        用途：写入一条审计事件。

        参数：
        - trace_id: 请求追踪 ID。
        - device_id: 设备 ID。
        - message_id: 消息 ID。
        - payload: 审计载荷字典。

        返回值：
        - 无。
        """
        with self._conn() as conn:
            conn.execute(
                """
                INSERT INTO audit_events
                (trace_id, device_id, message_id, ev, sid, locker_id, uid, code, http, net, door, cache, drop, created_at)
                VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
                """,
                (
                    trace_id,
                    device_id,
                    message_id,
                    payload.get("ev"),
                    payload.get("sid"),
                    payload.get("lockerId"),
                    payload.get("uid"),
                    payload.get("code"),
                    payload.get("http"),
                    payload.get("net"),
                    payload.get("door"),
                    payload.get("cache"),
                    payload.get("drop"),
                    self._now_iso(),
                ),
            )

    def cleanup_audit_events(self, retention_days: int) -> int:
        """
        用途：删除超过保留期的审计数据。

        参数：
        - retention_days: 保留天数。

        返回值：
        - int: 本次删除行数。
        """
        threshold = (
            datetime.now(timezone.utc) - timedelta(days=retention_days)
        ).replace(microsecond=0).isoformat()

        with self._conn() as conn:
            cur = conn.execute(
                "DELETE FROM audit_events WHERE created_at < ?",
                (threshold,),
            )
            return cur.rowcount

    def upsert_device(self, device_id: str, secret: str, status: int = 1) -> None:
        """
        用途：插入或更新设备信息。

        参数：
        - device_id: 设备 ID。
        - secret: 设备签名密钥。
        - status: 设备状态（1=启用）。

        返回值：
        - 无。
        """
        with self._conn() as conn:
            conn.execute(
                """
                INSERT INTO devices(device_id, secret, status, last_seen_at)
                VALUES(?, ?, ?, ?)
                ON CONFLICT(device_id)
                DO UPDATE SET secret=excluded.secret, status=excluded.status
                """,
                (device_id, secret, status, self._now_iso()),
            )

    def upsert_permission(
        self,
        uid_sha1: str,
        locker_id: str,
        active: int = 1,
        valid_from: Optional[str] = None,
        valid_to: Optional[str] = None,
    ) -> None:
        """
        用途：插入或更新卡权限。

        参数：
        - uid_sha1: UID SHA1。
        - locker_id: 门位 ID。
        - active: 是否启用权限（1=启用）。
        - valid_from: 生效时间（ISO 字符串，可空）。
        - valid_to: 失效时间（ISO 字符串，可空）。

        返回值：
        - 无。
        """
        with self._conn() as conn:
            conn.execute(
                """
                INSERT INTO card_permissions(uid_sha1, locker_id, active, valid_from, valid_to)
                VALUES(?, ?, ?, ?, ?)
                ON CONFLICT(uid_sha1, locker_id)
                DO UPDATE SET active=excluded.active, valid_from=excluded.valid_from, valid_to=excluded.valid_to
                """,
                (uid_sha1, locker_id, active, valid_from, valid_to),
            )

    @staticmethod
    def _now_iso() -> str:
        """
        用途：生成 UTC ISO 时间字符串（秒级）。

        参数：
        - 无。

        返回值：
        - str: 例如 `2026-03-03T12:00:00+00:00`。
        """
        return datetime.now(timezone.utc).replace(microsecond=0).isoformat()
