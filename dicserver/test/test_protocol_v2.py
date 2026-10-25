#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""v2 变长分帧协议的端到端验证（服务端侧）。

对照 test_dict.py（走 v1 定长 1316 字节），这里全程用 v2 帧，
验证：正常往返、半包拼包、两帧粘包、非法帧被拒、以及和 v1 的带宽对比。

    cd <build>/test && DICT_HOST=127.0.0.1 DICT_PORT=18080 python3 test_protocol_v2.py
"""
import os
import socket
import struct
import sys
import time

HOST = os.environ.get("DICT_HOST", "127.0.0.1")
PORT = int(os.environ.get("DICT_PORT", "8080"))

MAGIC = 0x44494354  # "DICT"
HEADER = 16
R, L, S, A = 1, 2, 4, 6

PASS, FAIL = [], []


def check(name, cond, detail=""):
    (PASS if cond else FAIL).append(name)
    print(("%s  %s  %s" % ("PASS" if cond else "FAIL", name, detail)))


def frame(mtype, name="", text=""):
    """编码一帧 v2：magic(4) bodyLen(4) | type(4) nameLen(2) textLen(2) name text

    body 只含 type/nameLen/textLen/payload 三部分，bodyLen = 8 + nameLen + textLen，
    与 shared/protocol_frame.hpp 里 encode() 的布局一致。
    """
    nb = name.encode("utf-8")
    tb = text.encode("utf-8")
    body = struct.pack("!IHH", mtype, len(nb), len(tb)) + nb + tb
    return struct.pack("!II", MAGIC, len(body)) + body


def recv_exact(sock, n, timeout=5):
    sock.settimeout(timeout)
    data = b""
    while len(data) < n:
        chunk = sock.recv(n - len(data))
        if not chunk:
            break
        data += chunk
    return data


def recv_frame(sock):
    """收一整帧。

    帧总长 = 8 + bodyLen。16 字节固定头里已经包含 type/nameLen/textLen，
    所以读完头之后还要读的只有 payload，长度是 bodyLen - 8；
    按 bodyLen 去读会多读 8 字节，第二次 recv 就会一直等下去直到超时。
    """
    head = recv_exact(sock, HEADER)
    if len(head) < HEADER:
        return None
    magic, body_len = struct.unpack("!II", head[:8])
    if magic != MAGIC:
        return ("BAD_MAGIC", magic, b"", b"")
    mtype, name_len, text_len = struct.unpack("!IHH", head[8:16])
    payload = recv_exact(sock, body_len - 8)
    name = payload[:name_len].decode("utf-8", "replace")
    text = payload[name_len:name_len + text_len].decode("utf-8", "replace")
    return (mtype, name, text)


def conn():
    return socket.create_connection((HOST, PORT), timeout=5)


def wait_server():
    for _ in range(60):
        try:
            s = conn()
            s.close()
            return
        except OSError:
            time.sleep(1)
    print("server not up")
    sys.exit(1)


wait_server()

# ---------- 1. 心跳：v2 往返，且帧远小于 v1 的 1316 字节 ----------
s = conn()
req = frame(A, "hbuser", "PING")
s.sendall(req)
rep = recv_frame(s)
check("v2 heartbeat 回帧 magic 正确", isinstance(rep, tuple) and rep[0] != "BAD_MAGIC", "rep=%r" % (rep,))
check("v2 heartbeat type=6", rep is not None and rep[0] == A, "type=%r" % (rep[0] if rep else None))
check("v2 heartbeat text=ALIVE", rep is not None and rep[2] == "ALIVE", "text=%r" % (rep[2] if rep else None))

# 带宽对比：同样的心跳，v1 要发 1316 字节请求 + 收 1316 字节响应
v1_bytes = 1316 * 2
v2_bytes = len(req) + (HEADER + 4 + len("hbuser") + len("ALIVE"))
check("心跳包体积显著小于 v1", v2_bytes * 10 < v1_bytes,
      "v2=%dB vs v1=%dB（省 %.1f%%）" % (v2_bytes, v1_bytes, 100.0 * (1 - v2_bytes / v1_bytes)))
s.close()

# ---------- 2. 注册 + 登录 + 查询全链路 ----------
user = "v2_%d" % int(time.time())
s = conn()
s.sendall(frame(R, user, "pass123"))
rep = recv_frame(s)
check("v2 注册返回 SUCCESS", rep is not None and rep[2].startswith("SUCCESS:"), "text=%r" % (rep[2] if rep else None))

s.sendall(frame(L, user, "pass123"))
rep = recv_frame(s)
check("v2 登录返回 SUCCESS", rep is not None and rep[2].startswith("SUCCESS:"), "text=%r" % (rep[2] if rep else None))

s.sendall(frame(S, user, "hello"))
rep = recv_frame(s)
check("v2 查词返回释义", rep is not None and rep[0] == S and rep[2] != "NOT_FOUND",
      "text=%r" % (rep[2][:40] if rep else None))
s.close()

# ---------- 3. 半包：一帧拆成三段发，中间各停 0.3s ----------
s = conn()
pkt = frame(A, "frag", "PING")
third = len(pkt) // 3
s.sendall(pkt[:third])
time.sleep(0.3)
s.sendall(pkt[third:third * 2])
time.sleep(0.3)
s.sendall(pkt[third * 2:])
rep = recv_frame(s)
check("v2 半包拼包后能正确回 ACK", rep is not None and rep[0] == A and rep[2] == "ALIVE",
      "rep=%r" % (rep,))
s.close()

# ---------- 4. 粘包：两帧一次性发出，应拆出两个独立响应 ----------
# 用注册而不是心跳：心跳响应按设计不回显 name（服务端 responseHeartbeat
# 里明确 (void)msg），只有回显 name 的请求才能验证两帧没有被串行错。
s = conn()
u1 = "co1_%d" % int(time.time())
u2 = "co2_%d" % int(time.time())
s.sendall(frame(R, u1, "p1") + frame(R, u2, "p2"))
r1 = recv_frame(s)
r2 = recv_frame(s)
check("v2 粘包拆出第 1 帧", r1 is not None and r1[1] == u1, "name=%r" % (r1[1] if r1 else None))
check("v2 粘包拆出第 2 帧", r2 is not None and r2[1] == u2, "name=%r" % (r2[1] if r2 else None))
check("v2 粘包第 1 帧是注册响应", r1 is not None and r1[0] == R, "type=%r" % (r1[0] if r1 else None))
s.close()

# ---------- 5. 中文用户名/释义按字节长度正确切分 ----------
s = conn()
cn_user = "中文用户"
s.sendall(frame(R, cn_user, "密码123"))
rep = recv_frame(s)
check("v2 中文用户名注册成功", rep is not None and rep[1] == cn_user, "name=%r" % (rep[1] if rep else None))
s.close()

# ---------- 6. 非法帧：magic 不对，服务端应断开而不是崩溃 ----------
s = conn()
bad = frame(A, "bad", "PING")
bad = b"XXXX" + bad[4:]  # 破坏 magic
s.sendall(bad)
# 服务端收到非法帧头会关闭连接，读应返回空；同时服务端本身必须还活着
try:
    tail = recv_exact(s, 64, timeout=3)
except socket.timeout:
    tail = b"<timeout>"
check("非法 magic 不会收到正常响应", tail in (b"", b"<timeout>"), "recv=%r" % tail[:20])
s.close()

# 服务端存活验证：另开连接正常心跳
s = conn()
s.sendall(frame(A, "alive", "PING"))
rep = recv_frame(s)
check("非法帧之后服务端仍存活", rep is not None and rep[2] == "ALIVE", "text=%r" % (rep[2] if rep else None))
s.close()

# ---------- 7. bodyLen 越界：超大长度不应让服务端分配巨量内存 ----------
s = conn()
evil = struct.pack("!II", MAGIC, 0xFFFFFFF) + b"\x00" * 8
s.sendall(evil)
try:
    tail = recv_exact(s, 64, timeout=3)
except socket.timeout:
    tail = b"<timeout>"
check("bodyLen 越界被拒", tail in (b"", b"<timeout>"), "recv=%r" % tail[:20])
s.close()

s = conn()
s.sendall(frame(A, "alive2", "PING"))
rep = recv_frame(s)
check("越界帧之后服务端仍存活", rep is not None and rep[2] == "ALIVE", "text=%r" % (rep[2] if rep else None))
s.close()

print("\nPASS=%d FAIL=%d" % (len(PASS), len(FAIL)))
sys.exit(0 if not FAIL else 1)
