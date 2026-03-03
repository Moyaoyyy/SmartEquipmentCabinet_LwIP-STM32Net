"""
文件作用：FastAPI 服务主入口。

主要职责：
- 加载配置并初始化 SQLite 仓储。
- 注册 HTTP 路由（/api/uplink、/healthz）。
- 在启动时创建后台清理协程，在关闭时安全取消。

依赖/调用关系：
- Uvicorn 通过 `app.main:app` 导入该文件。
- 调用 `router_uplink` 处理上报请求。
- 调用 `cleanup.run_cleanup_loop` 定期清理历史审计数据。
"""

import asyncio
import contextlib
import logging

from fastapi import FastAPI

from .cleanup import run_cleanup_loop
from .config import load_settings
from .repo_sqlite import SQLiteRepo
from .router_uplink import router
from .security import NonceStore


def _setup_logging(level: str) -> None:
    """
    用途：初始化全局日志格式与日志级别。

    参数：
    - level: 日志级别字符串（如 INFO/DEBUG/WARN）。

    返回值：
    - 无。
    """
    logging.basicConfig(
        level=getattr(logging, level, logging.INFO),
        format="%(asctime)s %(levelname)s %(name)s %(message)s",
    )


def create_app() -> FastAPI:
    """
    用途：创建并配置 FastAPI 应用实例。

    参数：
    - 无。

    返回值：
    - FastAPI: 已完成配置的应用对象。

    边界行为：
    - 若数据库文件不存在，会在初始化阶段自动创建。
    """
    settings = load_settings()
    _setup_logging(settings.log_level)

    # 初始化数据库访问层，并确保表结构存在。
    repo = SQLiteRepo(settings.db_path)
    repo.init_db()

    app = FastAPI(title="RFID Uplink Server", version="0.1.0")

    # 共享状态对象挂载到 app.state，供路由函数读取。
    app.state.settings = settings
    app.state.repo = repo
    app.state.nonce_store = NonceStore(ttl_sec=settings.nonce_ttl_sec)
    app.state.cleanup_task = None

    # 注册上报路由。
    app.include_router(router)

    @app.get("/healthz")
    async def healthz():
        """
        用途：健康检查接口。

        参数：
        - 无。

        返回值：
        - dict: 固定返回 {"ok": True}。
        """
        return {"ok": True}

    @app.on_event("startup")
    async def startup_event() -> None:
        """
        用途：应用启动事件处理。

        参数：
        - 无。

        返回值：
        - 无。

        说明：
        - 启动后台协程，每日执行一次审计数据清理。
        """
        app.state.cleanup_task = asyncio.create_task(run_cleanup_loop(repo, settings))

    @app.on_event("shutdown")
    async def shutdown_event() -> None:
        """
        用途：应用关闭事件处理。

        参数：
        - 无。

        返回值：
        - 无。

        说明：
        - 安全取消后台清理协程，避免进程退出时挂起任务。
        """
        task = app.state.cleanup_task
        if task is not None:
            task.cancel()
            with contextlib.suppress(asyncio.CancelledError):
                await task

    return app


# Uvicorn 默认导入对象：uvicorn app.main:app
app = create_app()
