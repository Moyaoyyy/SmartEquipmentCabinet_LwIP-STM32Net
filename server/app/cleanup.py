"""
文件作用：审计数据保留清理任务。

主要职责：
- 按固定周期（24小时）清理超过保留天数的审计数据。
- 将清理结果写入日志，便于运维观察。

依赖/调用关系：
- 由 `main.py` 在 startup 事件中创建后台协程。
"""

import asyncio
import logging

from .config import Settings
from .repo_sqlite import SQLiteRepo


async def run_cleanup_loop(repo: SQLiteRepo, settings: Settings) -> None:
    """
    用途：循环执行历史审计清理。

    参数：
    - repo: SQLite 仓储实例。
    - settings: 配置对象，提供保留天数。

    返回值：
    - 无（长期运行协程）。

    边界行为：
    - 任意一次清理异常不会中断循环，会记录异常后进入下一轮等待。
    """
    while True:
        try:
            deleted = repo.cleanup_audit_events(settings.data_retention_days)
            logging.getLogger("uplink.cleanup").info(
                "cleanup completed, deleted=%s retention_days=%s",
                deleted,
                settings.data_retention_days,
            )
        except Exception as exc:  # pragma: no cover
            logging.getLogger("uplink.cleanup").exception("cleanup failed: %s", exc)

        # 每 24 小时执行一次，满足“30天轻量留存”策略。
        await asyncio.sleep(24 * 60 * 60)
