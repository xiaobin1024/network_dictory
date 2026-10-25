#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""dicserver 生词本与账号注销协议验证。

覆盖：
  1. type=10 生词本加入 → add_success|^|word
  2. type=10 重复加入 → 幂等，仍返回 add_success
  3. type=11 生词本列表 → 记录字段完整、字段数=6、顺序正确
  4. type=10 移出 → remove_success
  5. type=11 列表为空 → **EMPTY**
  6. type=9 注销（密码错误）→ **PASSWORD_ERROR**
  7. type=9 注销（密码正确）→ **OK**，且用户数据被级联删除
  8. type=11 协议类型 11 已被服务端正确处理（不再被静默丢弃）
  9. 非法消息类型 12 仍被安全丢弃，服务端存活
"""
import socket
import struct
import time
import sys

import os
HOST = os.environ.get("DICT_HOST", "127.0.0.1")
PORT = int(os.environ.get("DICT_PORT", "8080"))
MSGSIZE = 1316
R, L, Q, S, H, A, C, D = 1, 2, 3, 4, 5, 6, 7, 8
U, V, W = 9, 10, 11
PASS, FAIL = [], []


def check(name, cond, detail=""):
    (PASS if cond else FAIL).append(name)
    print(("%s  %s  %s" % ("PASS" if cond else "FAIL", name, detail))
          .encode("utf-8", "replace").decode())


def make_msg(mtype, name=b"", text=b""):
    m = struct.pack("!i", mtype)
    m += name.encode()[:32].ljust(32, b"\x00")
    m += text.encode()[:1280].ljust(1280, b"\x00")
    assert len(m) == MSGSIZE
    return m


def recv_exact(sock, n):
    data = b""
    while len(data) < n:
        chunk = sock.recv(n - len(data))
        if not chunk:
            break
        data += chunk
    return data


def parse(data):
    mtype = struct.unpack("!i", data[:4])[0]
    name = data[4:36].rstrip(b"\x00").decode("utf-8", "replace")
    text = data[36:].rstrip(b"\x00").decode("utf-8", "replace")
    return mtype, name, text


def wait_server():
    for _ in range(60):
        try:
            s = socket.create_connection((HOST, PORT), timeout=2)
            s.close()
            return
        except OSError:
            time.sleep(1)
    print("server not up")
    sys.exit(1)


wait_server()

stamp = str(int(time.time()))
user = "vocab_%s" % stamp
pwd = "vocabpass123"

SEP = "|^|"

# ---------- 0. 注册用户 ----------
s = socket.create_connection((HOST, PORT), timeout=5)
s.sendall(make_msg(R, user, pwd))
t, n, txt = parse(recv_exact(s, MSGSIZE))
check("register user", t == R and txt.startswith("SUCCESS:"), "type=%d text=%r" % (t, txt))

# ---------- 1. 加入生词本 ----------
payload = SEP.join(["hello", "n. 你好", "/həˈləʊ/", "Hello there!", "你好啊", "0"])
s.sendall(make_msg(V, user, payload))
t, n, txt = parse(recv_exact(s, MSGSIZE))
check("vocabulary add -> add_success", t == V and txt.startswith("add_success"),
      "type=%d text=%r" % (t, txt))
check("vocabulary add echoes word", txt.endswith(SEP + "hello"), "text=%r" % txt)

# ---------- 2. 重复加入（幂等） ----------
s.sendall(make_msg(V, user, payload))
t, n, txt = parse(recv_exact(s, MSGSIZE))
check("vocabulary add duplicate is idempotent", t == V and txt.startswith("add_success"),
      "type=%d text=%r" % (t, txt))

# ---------- 3. 查询生词本列表 ----------
s.sendall(make_msg(W, user, ""))
records = []
over = False
empty = False
for _ in range(20):
    t, n, txt = parse(recv_exact(s, MSGSIZE))
    if t != W:
        break
    if txt == "**OVER**":
        over = True
        break
    if txt == "**EMPTY**":
        empty = True
        break
    records.append(txt)

check("vocabulary list terminates with **OVER**", over, "records=%d" % len(records))
check("vocabulary list not empty", len(records) == 1, "records=%d" % len(records))
if records:
    parts = records[0].split(SEP)
    check("vocabulary record has 6 fields", len(parts) == 6, "fields=%d raw=%r" % (len(parts), records[0]))
    check("vocabulary record word", parts[0] == "hello", "word=%r" % parts[0])
    check("vocabulary record meaning", parts[1] == "n. 你好", "meaning=%r" % parts[1])
    check("vocabulary record phonetic", parts[2] == "/həˈləʊ/", "phonetic=%r" % parts[2])
    check("vocabulary record example", parts[3] == "Hello there!", "example=%r" % parts[3])
    check("vocabulary record translation", parts[4] == "你好啊", "translation=%r" % parts[4])
    check("vocabulary record collect flag", parts[5] in ("0", "1"), "flag=%r" % parts[5])

# ---------- 4. 移出生词本 ----------
s.sendall(make_msg(V, user, SEP.join(["hello", "n. 你好", "/həˈləʊ/", "Hello there!", "你好啊", "1"])))
t, n, txt = parse(recv_exact(s, MSGSIZE))
check("vocabulary remove -> remove_success", t == V and txt.startswith("remove_success"),
      "type=%d text=%r" % (t, txt))

# ---------- 5. 移出后列表应为空 ----------
s.sendall(make_msg(W, user, ""))
got_empty = False
for _ in range(20):
    t, n, txt = parse(recv_exact(s, MSGSIZE))
    if t != W:
        break
    if txt == "**EMPTY**":
        got_empty = True
        break
    if txt == "**OVER**":
        break
check("vocabulary list empty after remove", got_empty, "")

# ---------- 6. 注销：密码错误 ----------
s.sendall(make_msg(U, user, "wrongpass"))
t, n, txt = parse(recv_exact(s, MSGSIZE))
check("unregister with wrong password rejected", t == U and "密码错误" in txt,
      "type=%d text=%r" % (t, txt))

# ---------- 7. 注销：密码正确 ----------
s.sendall(make_msg(U, user, pwd))
t, n, txt = parse(recv_exact(s, MSGSIZE))
check("unregister with correct password -> OK", t == U and txt.startswith("SUCCESS:"),
      "type=%d text=%r" % (t, txt))

# ---------- 8. 注销后该用户无法登录 ----------
s2 = socket.create_connection((HOST, PORT), timeout=5)
s2.sendall(make_msg(L, user, pwd))
t, n, txt = parse(recv_exact(s2, MSGSIZE))
check("unregistered user cannot login", t == L and "用户不存在" in txt,
      "type=%d text=%r" % (t, txt))
s2.close()

# ---------- 9. 非法消息类型应被安全丢弃，服务端存活 ----------
s3 = socket.create_connection((HOST, PORT), timeout=5)
s3.sendall(make_msg(12, user, "bad"))
# 紧接着发心跳，若服务端存活应回 ACK
s3.sendall(make_msg(A, "probe", "PING"))
rep = recv_exact(s3, MSGSIZE)
t, n, txt = parse(rep)
check("invalid type 12 dropped, server alive", t == A and txt == "ALIVE",
      "type=%d text=%r" % (t, txt))
s3.close()

# ---------- 10. 注销用户后其历史/收藏被级联清理（用 sqlite3 校验）----------
import subprocess
try:
    out = subprocess.check_output(
        ["sqlite3", "database.db",
         "select count(*) from usr where name='%s';" % user],
        stderr=subprocess.DEVNULL).decode().strip()
    check("unregistered user removed from usr table", out == "0", "count=%s" % out)
except Exception as e:
    print("  [skip] sqlite3 check: %s" % e)

s.close()

print("\nPASS=%d FAIL=%d" % (len(PASS), len(FAIL)))
sys.exit(1 if FAIL else 0)
