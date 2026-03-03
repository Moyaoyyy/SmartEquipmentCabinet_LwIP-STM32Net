"""
文件作用：server.app 包初始化文件。

主要职责：
- 声明该目录为 Python 包，便于 `uvicorn app.main:app` 等方式导入。
- 当前不承载业务逻辑，业务入口位于 `app/main.py`。

依赖/调用关系：
- 被 `run_local.ps1` 触发的 Uvicorn 导入链间接使用。
"""
