"""
文件作用：初始化本机联调所需的演示数据。

主要职责：
- 初始化数据库表结构。
- 写入一个演示设备（`stm32f4`）。
- 写入两条演示权限（A01/A02）。

使用场景：
- 本机第一次联调前执行一次，确保服务有可判定的数据。
"""

from datetime import datetime, timedelta, timezone
from pathlib import Path
import sys

# 让脚本可从 `server/tools` 直接执行并导入 `app` 包。
ROOT_DIR = Path(__file__).resolve().parents[1]
if str(ROOT_DIR) not in sys.path:
    sys.path.insert(0, str(ROOT_DIR))

from app.config import load_settings
from app.repo_sqlite import SQLiteRepo


def main() -> None:
    """
    用途：写入演示设备与权限数据。

    参数：
    - 无。

    返回值：
    - 无（结果通过 stdout 打印）。
    """
    settings = load_settings()
    repo = SQLiteRepo(settings.db_path)
    repo.init_db()

    # 演示设备：供本机联调签名/设备识别使用。
    repo.upsert_device("stm32f4", "dev-secret-stm32f4", status=1)

    now = datetime.now(timezone.utc).replace(microsecond=0)
    valid_from = (now - timedelta(days=1)).isoformat()
    valid_to = (now + timedelta(days=30)).isoformat()

    # 演示权限：两张卡分别有 A01/A02 权限。
    repo.upsert_permission(
        uid_sha1="1111111111111111111111111111111111111111",
        locker_id="A01",
        active=1,
        valid_from=valid_from,
        valid_to=valid_to,
    )
    repo.upsert_permission(
        uid_sha1="2222222222222222222222222222222222222222",
        locker_id="A02",
        active=1,
        valid_from=valid_from,
        valid_to=valid_to,
    )

    print("seed completed")
    print(f"db={Path(settings.db_path)}")


if __name__ == "__main__":
    main()
