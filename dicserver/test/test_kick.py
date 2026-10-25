#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""验证：客户端掉网（无 FIN）后，服务端在约 90s 心跳超时后踢掉死连接并释放线程。"""
import socket
import struct
import time
import sys

import os
HOST = os.environ.get("DICT_HOST", "127.0.0.1")
PORT = int(os.environ.get("DICT_PORT", "8080"))
MSGSIZE = 1316

def make_msg(mtype, name=b"", text=b""):
    m = struct.pack("!i", mtype)
    m += name.encode()[:32].ljust(32, b"\x00")
    m += text.encode()[:1280].ljust(1280, b"\x00")
    return m

def recv_exact(sock, n):
    data = b""
    while len(data) < n:
        chunk = sock.recv(n - len(data))
        if not chunk:
            break
        data += chunk
    return data

# 先登录建立一个用户，再模拟"拔掉网线"：登录后不再发任何字节
s = socket.create_connection((HOST, PORT), timeout=10)
s.sendall(make_msg(1, "kick_user", "pw"))   # 注册
recv_exact(s, MSGSIZE)
s.sendall(make_msg(2, "kick_user", "pw"))   # 登录
recv_exact(s, MSGSIZE)

t0 = time.time()
closed_at = None
s.settimeout(125)
try:
    while True:
        chunk = s.recv(16)
        if not chunk:
            closed_at = time.time() - t0
            break
except socket.timeout:
    pass

if closed_at is None:
    print("FAIL  dead connection NOT kicked within 125s")
    sys.exit(1)

print("dead connection closed by server after %.1fs" % closed_at)
ok = 80 <= closed_at <= 110
print(("%s  kick time in [80,110]s (threshold=90s, recv timeout=10s)"
       % ("PASS" if ok else "FAIL")))

# 踢掉后服务端线程应已释放，立即建一个新连接并心跳，验证服务正常
s2 = socket.create_connection((HOST, PORT), timeout=5)
s2.sendall(make_msg(6, "after", "PING"))
rep = recv_exact(s2, MSGSIZE)
alive = rep[36:].rstrip(b"\x00") == b"ALIVE"
print("%s  server still serves after kick" % ("PASS" if alive else "FAIL"))
s2.close()

sys.exit(0 if (ok and alive) else 1)
