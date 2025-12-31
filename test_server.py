"""
简单 HTTP 监听服务（用于验证 STM32 uplink 的 HTTP JSON POST）

用途：
- 监听 0.0.0.0:8080
- 打印收到的 POST body（JSON 字符串）
- 返回 HTTP 200 + {"code":0}，让开发板判定成功并出队
"""

import socket
import socketserver
from http.server import HTTPServer, BaseHTTPRequestHandler


# 修复 Windows 主机名包含非 ASCII 字符导致的 UnicodeDecodeError
class FixedHTTPServer(HTTPServer):
    def server_bind(self):
        # 跳过 getfqdn 调用，直接绑定
        socketserver.TCPServer.server_bind(self)
        host, port = self.server_address[:2]
        self.server_name = host if host else "localhost"
        self.server_port = port


class Handler(BaseHTTPRequestHandler):
    def do_POST(self):
        # 读取 Content-Length（有些客户端/异常情况下可能不存在，做容错）
        length = int(self.headers.get("Content-Length", "0"))
        body = self.rfile.read(length) if length > 0 else b""

        try:
            print(f"[POST {self.path}] Received: {body.decode('utf-8', errors='replace')}")
        except Exception:
            print("[POST] Received: <decode failed>")

        # 返回 HTTP 200 + JSON
        self.send_response(200)
        self.send_header("Content-Type", "application/json")
        self.end_headers()
        self.wfile.write(b'{"code":0}')

    def log_message(self, format, *args):
        # 关闭 BaseHTTPRequestHandler 默认的每请求一行日志，避免刷屏
        return


if __name__ == "__main__":
    BIND_IP = "0.0.0.0"  # 监听所有网卡
    BIND_PORT = 8080

    print(f"Starting server on {BIND_IP}:{BIND_PORT} ...")
    print("Press Ctrl+C to stop.\n")

    server = FixedHTTPServer((BIND_IP, BIND_PORT), Handler)
    server.serve_forever()
