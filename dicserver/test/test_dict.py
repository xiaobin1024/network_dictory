#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""dicserver 协议端到端验证：心跳ACK、半包拼包、注册/登录/查询、空闲不空转。"""
import socket
import struct
import time
import sys
import os
import subprocess

import os
HOST = os.environ.get("DICT_HOST", "127.0.0.1")
PORT = int(os.environ.get("DICT_PORT", "8080"))
MSGSIZE = 1316
R, L, Q, S, H, A, C, D = 1, 2, 3, 4, 5, 6, 7, 8
PASS, FAIL = [], []

def check(name, cond, detail=""):
    (PASS if cond else FAIL).append(name)
    print(("%s  %s  %s" % ("PASS" if cond else "FAIL", name, detail)).encode("utf-8", "replace").decode())

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
            s = socket.create_connection((HOST, PORT), timeout=2); s.close(); return
        except OSError:
            time.sleep(1)
    print("server not up"); sys.exit(1)

wait_server()

# ---------- 1. 心跳 ACK ----------
s = socket.create_connection((HOST, PORT), timeout=5)
s.sendall(make_msg(A, "hbuser", "PING"))
rep = recv_exact(s, MSGSIZE)
t, n, txt = parse(rep)
check("heartbeat ACK type=6", t == A, "type=%d" % t)
check("heartbeat ACK text=ALIVE", txt == "ALIVE", "text=%r" % txt)
s.close()

# ---------- 2. 半包/分片：注册包拆成两段，中间间隔 0.5s ----------
user = "frag_%d" % int(time.time())
pkt = make_msg(R, user, "pass123")
s2 = socket.create_connection((HOST, PORT), timeout=5)
s2.sendall(pkt[:100])
time.sleep(0.5)
s2.sendall(pkt[100:])
rep = recv_exact(s2, MSGSIZE)
t, n, txt = parse(rep)
check("fragment register ACK type=1", t == R, "type=%d" % t)
check("fragment register ACK name", n == user, "name=%s" % n)
check("fragment register SUCCESS payload", txt.startswith("SUCCESS:") and len(txt.split(":")) == 5,
      "text=%r" % txt)
s2.close()

# ---------- 2b. 20 字符用户名边界回归 ----------
# 历史缺陷：协议 name 字段仅 20 字节，20 字符用户名恰好填满而没有 '\0'，
# 服务端按 C 字符串读取时被截断，导致注册后无法登录。扩容后必须完整承载。
# 注意：账号可能已存在（重复执行测试），此时注册返回 ERROR 亦可接受，
# 关键断言是 name 完整回显且登录能命中未被截断的账号。
long_user = "b" + "c" * 19  # 20 字符，字母开头
assert len(long_user) == 20
s2b = socket.create_connection((HOST, PORT), timeout=5)
s2b.sendall(make_msg(R, long_user, "boundary123"))
rep = recv_exact(s2b, MSGSIZE)
t, n, txt = parse(rep)
check("20-char username register ACK name intact", t == R and n == long_user,
      "name=%s (len=%d)" % (n, len(n)))
# 用完整 20 字符用户名登录，必须命中（未被截断的账号）
s2b.sendall(make_msg(L, long_user, "boundary123"))
rep = recv_exact(s2b, MSGSIZE)
t, n, txt = parse(rep)
check("20-char username login succeeds", t == L and txt.startswith("SUCCESS:")
      and txt.split(":")[1] == long_user, "text=%r" % txt)
s2b.close()

# ---------- 3. 再用 1 字节/400 字节/剩余 三段更刁钻地分片注册 ----------
user2 = "frag2_%d" % int(time.time())
pkt = make_msg(R, user2, "pw456")
s3 = socket.create_connection((HOST, PORT), timeout=5)
s3.sendall(pkt[:1]); time.sleep(0.3)
s3.sendall(pkt[1:401]); time.sleep(0.3)
s3.sendall(pkt[401:])
rep = recv_exact(s3, MSGSIZE)
t, n, txt = parse(rep)
check("tiny-fragment register ACK", t == R and n == user2, "type=%d name=%s" % (t, n))
# 同一连接接着登录（验证拼包缓冲在多帧复用下正确）
s3.sendall(make_msg(L, user2, "pw456"))
rep = recv_exact(s3, MSGSIZE)
t, n, txt = parse(rep)
check("login on same connection", t == L, "type=%d" % t)
# 登录响应结构：SUCCESS:<用户名>:<密码>:<用户ID>:<角色>，ID/角色必须为数字
parts = txt.split(":")
check("login SUCCESS payload has 5 fields", len(parts) == 5 and parts[0] == "SUCCESS"
      and parts[1] == user2 and parts[2] == "pw456", "text=%r" % txt)
check("login payload userId is numeric", len(parts) >= 4 and parts[3].isdigit(),
      "userId=%r" % (parts[3] if len(parts) > 3 else None))
check("login payload role is numeric", len(parts) >= 5 and parts[4].isdigit(),
      "role=%r" % (parts[4] if len(parts) > 4 else None))
s3.close()

# ---------- 4. 错误密码登录应返回失败 ----------
s4 = socket.create_connection((HOST, PORT), timeout=5)
s4.sendall(make_msg(L, user2, "wrongpw"))
rep = recv_exact(s4, MSGSIZE)
t, n, txt = parse(rep)
check("wrong-password login rejected", t == L and "密码错误" in txt,
      "type=%d text=%r" % (t, txt))
s4.close()

# ---------- 5. 查询单词（正常登录用户） ----------
s5 = socket.create_connection((HOST, PORT), timeout=5)
s5.sendall(make_msg(L, user, "pass123"))
recv_exact(s5, MSGSIZE)
# 取词典里第一个词，保证能查到
word = "hello"
import os as _os
with open(_os.path.join(_os.path.dirname(_os.path.abspath(__file__)), "..", "dict.txt"),
          "r", encoding="utf-8", errors="replace") as f:
    first = f.readline().strip()
    if first:
        word = first.split()[0]
s5.sendall(make_msg(S, user, word))
rep = recv_exact(s5, MSGSIZE)
t, n, txt = parse(rep)
# 查询响应协议：单词|^|释义|^|音标|^|例句|^|翻译|^|收藏标志|^|生词本标志，共 7 字段
SEP = "|^|"
qparts = txt.split(SEP)
check("query returns 7-field payload", t == S and len(qparts) == 7,
      "fields=%d text=%r" % (len(qparts), txt[:60]))
check("query payload word/meaning", len(qparts) >= 2 and qparts[0] == word and qparts[1] != "",
      "word=%r meaning=%r" % (qparts[0] if qparts else None, qparts[1] if len(qparts) > 1 else None))
check("query payload flags numeric",
      len(qparts) == 7 and qparts[5] in ("0", "1") and qparts[6] in ("0", "1"),
      "collect=%r vocab=%r" % (qparts[5] if len(qparts) > 5 else None,
                               qparts[6] if len(qparts) > 6 else None))
# 未收录的单词必须返回精确的 NOT_FOUND
s5.sendall(make_msg(S, user, "zzz_not_a_word_zzz"))
rep = recv_exact(s5, MSGSIZE)
t, n, txt = parse(rep)
check("query unknown word -> NOT_FOUND", t == S and txt == "NOT_FOUND", "text=%r" % txt)

# ---------- 5b. 收藏 toggle：加入 -> 查询标志变 1 -> 取消 -> 标志变 0 ----------
collect_payload = SEP.join([word, "n.测试", "/test/", "test example", "测试翻译", "0"])
s5.sendall(make_msg(C, user, collect_payload))
rep = recv_exact(s5, MSGSIZE)
t, n, txt = parse(rep)
check("collect add -> collect_success|^|word",
      t == C and txt == "collect_success" + SEP + word, "text=%r" % txt)
# 再次查询，收藏标志应为 1
s5.sendall(make_msg(S, user, word))
rep = recv_exact(s5, MSGSIZE)
t, n, txt = parse(rep)
qparts = txt.split(SEP)
check("query collect flag = 1 after collect",
      t == S and len(qparts) == 7 and qparts[5] == "1", "collect=%r" % (qparts[5] if len(qparts) > 5 else None))
# 重复收藏同一单词 = 取消收藏
s5.sendall(make_msg(C, user, collect_payload))
rep = recv_exact(s5, MSGSIZE)
t, n, txt = parse(rep)
check("collect toggle -> cancel_success|^|word",
      t == C and txt == "cancel_success" + SEP + word, "text=%r" % txt)
s5.sendall(make_msg(S, user, word))
rep = recv_exact(s5, MSGSIZE)
t, n, txt = parse(rep)
qparts = txt.split(SEP)
check("query collect flag = 0 after cancel",
      t == S and len(qparts) == 7 and qparts[5] == "0", "collect=%r" % (qparts[5] if len(qparts) > 5 else None))
# 收藏列表：应为该单词一条记录，6 字段
s5.sendall(make_msg(C, user, collect_payload))
recv_exact(s5, MSGSIZE)
s5.sendall(make_msg(D, user, ""))
records = []
over = False
for _ in range(20):
    t, n, txt = parse(recv_exact(s5, MSGSIZE))
    if t != D:
        break
    if txt == "**OVER**":
        over = True
        break
    if txt == "**EMPTY**":
        break
    records.append(txt)
check("collect list terminates with **OVER**", over, "records=%d" % len(records))
if records:
    cparts = records[0].split(SEP)
    check("collect record has 6 fields", len(cparts) == 6, "fields=%d raw=%r" % (len(cparts), records[0][:60]))
    check("collect record word matches", len(cparts) >= 1 and cparts[0] == word, "word=%r" % (cparts[0] if cparts else None))
# 清理：取消收藏，避免影响后续用例
s5.sendall(make_msg(C, user, collect_payload))
recv_exact(s5, MSGSIZE)
s5.close()

# ---------- 6. 两条空闲连接不应造成 CPU 空转 ----------
idle = [socket.create_connection((HOST, PORT), timeout=5) for _ in range(2)]
time.sleep(2)
pid = subprocess.check_output(["pidof", "server"]).split()[0].decode()
out = subprocess.check_output(["top", "-b", "-n", "2", "-d", "4", "-p", pid],
                              stderr=subprocess.DEVNULL).decode()
proc_lines = [ln for ln in out.splitlines() if ln.strip().startswith(pid)]
cpu = float(proc_lines[-1].split()[8])  # %CPU 列
check("idle CPU near zero (%.1f%%)" % cpu, cpu < 5.0, "cpu=%s%%" % cpu)

# ---------- 7. 粘包：两条报文一次性 write，服务端应解析为两帧 ----------
s7 = socket.create_connection((HOST, PORT), timeout=5)
double = make_msg(A, "cc", "PING") + make_msg(A, "cc", "PING")
s7.sendall(double)
r1 = recv_exact(s7, MSGSIZE)
r2 = recv_exact(s7, MSGSIZE)
t1, _, x1 = parse(r1); t2, _, x2 = parse(r2)
check("coalesced two frames -> 2 ACKs", t1 == A and t2 == A and x1 == "ALIVE" and x2 == "ALIVE",
      "%d/%d" % (t1, t2))
s7.close()
for c in idle:
    c.close()

print("\nPASS=%d FAIL=%d" % (len(PASS), len(FAIL)))
sys.exit(1 if FAIL else 0)
