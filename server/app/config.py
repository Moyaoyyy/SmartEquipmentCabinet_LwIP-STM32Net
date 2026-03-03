"""
文件作用：服务配置读取模块。

主要职责：
- 读取 `.env` 与系统环境变量。
- 将字符串配置转换为强类型配置对象 `Settings`。
- 统一管理服务监听地址、数据库路径、签名校验参数等。

依赖/调用关系：
- `main.py` 在启动时调用 `load_settings()`。
"""

import os
from dataclasses import dataclass
from pathlib import Path


def _load_env_file(env_path: Path) -> None:
    """
    用途：从 `.env` 文件加载键值对到环境变量。

    参数：
    - env_path: `.env` 文件路径。

    返回值：
    - 无。

    边界行为：
    - 文件不存在时直接返回，不抛异常。
    - 若系统环境已存在同名变量，则不覆盖（系统环境优先）。
    """
    if not env_path.exists():
        return

    for line in env_path.read_text(encoding="utf-8").splitlines():
        item = line.strip()
        # 跳过空行、注释行、非法行。
        if not item or item.startswith("#") or "=" not in item:
            continue

        key, value = item.split("=", 1)
        key = key.strip()
        value = value.strip()
        if key and key not in os.environ:
            os.environ[key] = value


def _to_bool(value: str, default: bool) -> bool:
    """
    用途：把字符串转换为布尔值。

    参数：
    - value: 输入字符串。
    - default: 输入为空时的默认值。

    返回值：
    - bool: 解析后的布尔值。
    """
    if value is None:
        return default
    return value.strip().lower() in {"1", "true", "yes", "on"}


def _to_int(value: str, default: int) -> int:
    """
    用途：把字符串转换为整数。

    参数：
    - value: 输入字符串。
    - default: 输入为空或非法时的默认值。

    返回值：
    - int: 解析后的整数值。
    """
    if value is None:
        return default
    try:
        return int(value)
    except ValueError:
        return default


@dataclass(frozen=True)
class Settings:
    """
    用途：集中描述服务运行时配置。

    字段说明：
    - app_host/app_port: HTTP 服务监听地址与端口。
    - db_path: SQLite 数据库文件路径。
    - data_retention_days: 审计数据保留天数。
    - signature_required: 是否强制设备签名校验。
    - signature_max_skew_sec: 签名时间戳允许偏差秒数。
    - nonce_ttl_sec: 防重放 nonce 的保留秒数。
    - log_level: 日志级别。
    """

    app_host: str
    app_port: int
    db_path: Path
    data_retention_days: int
    signature_required: bool
    signature_max_skew_sec: int
    nonce_ttl_sec: int
    log_level: str


def load_settings() -> Settings:
    """
    用途：加载并返回服务配置对象。

    参数：
    - 无。

    返回值：
    - Settings: 已解析并可直接使用的配置对象。

    说明：
    - 先读取 `server/.env`，再读取系统环境变量。
    - 相对数据库路径会被转换成以 `server/` 为基准的绝对路径，
      避免从不同工作目录启动时出现路径漂移。
    """
    root_dir = Path(__file__).resolve().parents[1]
    _load_env_file(root_dir / ".env")

    db_path_raw = os.getenv("DB_PATH", "./data/uplink.db")
    db_path = Path(db_path_raw)
    if not db_path.is_absolute():
        db_path = (root_dir / db_path).resolve()

    return Settings(
        app_host=os.getenv("APP_HOST", "0.0.0.0"),
        app_port=_to_int(os.getenv("APP_PORT"), 8080),
        db_path=db_path,
        data_retention_days=_to_int(os.getenv("DATA_RETENTION_DAYS"), 30),
        signature_required=_to_bool(os.getenv("SIGNATURE_REQUIRED"), False),
        signature_max_skew_sec=_to_int(os.getenv("SIGNATURE_MAX_SKEW_SEC"), 120),
        nonce_ttl_sec=_to_int(os.getenv("NONCE_TTL_SEC"), 300),
        log_level=os.getenv("LOG_LEVEL", "INFO").upper(),
    )
