"""
简单 HTTP 监听服务（用于验证 STM32 uplink 的 HTTP JSON POST）

用途：
- 监听 0.0.0.0:8080
- 打印收到的 POST body（JSON 字符串）
- 返回 HTTP 200 + {"code":0}，让开发板判定成功并出队
"""

from http.server import HTTPServer, BaseHTTPRequestHandler


class Handler(BaseHTTPRequestHandler):
    def do_POST(self):
        # 读取 Content-Length（有些客户端/异常情况下可能不存在，做容错）
        length = int(self.headers.get("Content-Length", "0"))
        body = self.rfile.read(length) if length > 0 else b""

        try:
            print(f"Received: {body.decode('utf-8', errors='replace')}")
        except Exception:
            print("Received: <decode failed>")

        # 返回 HTTP 200 + JSON
        self.send_response(200)
        self.send_header("Content-Type", "application/json")
        self.end_headers()
        self.wfile.write(b'{"code":0}')

    def log_message(self, format, *args):
        # 关闭 BaseHTTPRequestHandler 默认的每请求一行日志，避免刷屏
        return


if __name__ == "__main__":
    HTTPServer(("0.0.0.0", 8080), Handler).serve_forever()
