$ErrorActionPreference = "Stop"

# 进入 server 目录，确保后续相对路径一致。
Set-Location $PSScriptRoot

# 若虚拟环境不存在，则创建本地 venv。
if (-not (Test-Path ".venv")) {
    python -m venv .venv
}

$python = Join-Path $PSScriptRoot ".venv\Scripts\python.exe"

# 更新 pip 并安装依赖。
& $python -m pip install --upgrade pip
& $python -m pip install -r requirements.txt

# 首次启动自动复制 .env 模板。
if (-not (Test-Path ".env")) {
    Copy-Item .env.example .env
}

# 本机联调默认开启热重载。
& $python -m uvicorn app.main:app --host 0.0.0.0 --port 8080 --reload
