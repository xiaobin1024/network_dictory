#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
dicserver 查词链路压测。

测什么：
  多并发长连接持续发查词请求（type=4 S），统计 QPS 与延迟分布（avg/p50/p95/p99）。
  这是词典最核心的读路径，也是热点词缓存要优化的目标路径。

怎么跑：
  cd <build_dir>/test
  DICT_HOST=127.0.0.1 DICT_PORT=18080 python3 bench_query.py
  DICT_MODE=wide DICT_CONC=16 DICT_ROUNDS=300 python3 bench_query.py

环境变量：
  DICT_HOST/DICT_PORT  被测实例地址
  DICT_CONC            并发连接数（每连接一个工作线程），默认 16
  DICT_ROUNDS          每个连接的请求数（不含预热），默认 300
  DICT_HOTSET          热点词集大小，默认 200
  DICT_MODE            hot=请求集中在热点词集内（命中率高）；wide=每次换不同词（命中率低）
  DICT_WARMUP          每连接预热请求数（不计入统计，用于填充缓存），默认 50
  DICT_DICT_PATH       词典文件，用于取真实存在的词，默认 ../dict.txt
  DICT_OUT             若设置，把 JSON 结果追加写入该文件

注意：
  只统计"单词已找到"的请求——服务端对不存在的词不写缓存，
  用不存在的词压测会让缓存永不命中，测不出真实效果。
"""

import json
import os
import random
import socket
import struct
import sys
import threading
import time

HOST = os.environ.get("DICT_HOST", "127.0.0.1")
PORT = int(os.environ.get("DICT_PORT", "8080"))
CONC = int(os.environ.get("DICT_CONC", "16"))
ROUNDS = int(os.environ.get("DICT_ROUNDS", "300"))
HOTSET = int(os.environ.get("DICT_HOTSET", "200"))
MODE = os.environ.get("DICT_MODE", "hot")          # hot | wide
WARMUP = int(os.environ.get("DICT_WARMUP", "50"))
DICT_PATH = os.environ.get("DICT_DICT_PATH", "../dict.txt")
OUT_FILE = os.environ.get("DICT_OUT", "")

MSGSIZE = 1316
S = 4  # 查询


def make_msg(mtype, name, text):
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


def load_words(path, count):
    """从词典文件里取 count 个真实存在的单词（按文件均匀抽样，避免全落在字母 a 段）。"""
    words = []
    try:
        with open(path, "r", encoding="utf-8", errors="ignore") as fh:
            lines = fh.readlines()
    except OSError:
        print("无法读取词典文件：%s（用 DICT_DICT_PATH 指定）" % path)
        sys.exit(1)

    total = len(lines)
    if total == 0:
        print("词典文件为空：%s" % path)
        sys.exit(1)

    step = max(1, total // count)
    for i in range(0, min(total, count * step), step):
        line = lines[i].strip()
        if not line:
            continue
        word = line.split(" ")[0]
        if word:
            words.append(word)
        if len(words) >= count:
            break
    return words


def percentile(sorted_vals, p):
    if not sorted_vals:
        return 0.0
    idx = int(len(sorted_vals) * p / 100.0)
    idx = min(idx, len(sorted_vals) - 1)
    return sorted_vals[idx]


def worker(wid, word_pool, results, errors):
    """一个并发连接：预热 -> 计时请求，逐条记录单请求耗时（毫秒）。"""
    latencies = []
    try:
        sock = socket.create_connection((HOST, PORT), timeout=30)
    except OSError as e:
        errors.append("worker %d connect failed: %s" % (wid, e))
        results[wid] = latencies
        return

    name = "bench_%d" % wid
    try:
        # 预热：填充服务端缓存与 SQLite page cache，不计入统计
        for _ in range(WARMUP):
            w = random.choice(word_pool) if MODE == "hot" else random.choice(word_pool)
            sock.sendall(make_msg(S, name, w))
            rep = recv_exact(sock, MSGSIZE)
            if len(rep) != MSGSIZE:
                errors.append("worker %d warmup short reply" % wid)
                break

        for _ in range(ROUNDS):
            w = random.choice(word_pool)
            t0 = time.perf_counter()
            sock.sendall(make_msg(S, name, w))
            rep = recv_exact(sock, MSGSIZE)
            dt = (time.perf_counter() - t0) * 1000.0
            if len(rep) != MSGSIZE:
                errors.append("worker %d short reply" % wid)
                break
            latencies.append(dt)
    except OSError as e:
        errors.append("worker %d: %s" % (wid, e))
    finally:
        sock.close()
    results[wid] = latencies


def main():
    random.seed(20261025)

    if MODE == "hot":
        pool_size = HOTSET
    else:
        # wide：词池远大于缓存容量，命中率天然很低
        pool_size = max(HOTSET * 20, 20000)
    word_pool = load_words(DICT_PATH, pool_size)
    print("词池大小=%d（mode=%s）" % (len(word_pool), MODE))

    # 探活，避免服务端没起来时压出一堆连接错误
    try:
        probe = socket.create_connection((HOST, PORT), timeout=5)
        probe.close()
    except OSError:
        print("服务端不可达：%s:%d" % (HOST, PORT))
        sys.exit(1)

    results = {}
    errors = []
    threads = []
    wall_t0 = time.perf_counter()
    for i in range(CONC):
        t = threading.Thread(target=worker, args=(i, word_pool, results, errors))
        t.start()
        threads.append(t)
    for t in threads:
        t.join()
    wall = time.perf_counter() - wall_t0

    all_lat = []
    for v in results.values():
        all_lat.extend(v)
    all_lat.sort()

    total = len(all_lat)
    if total == 0:
        print("没有任何成功的请求")
        if errors:
            print("\n".join(errors[:10]))
        sys.exit(1)

    qps = total / wall if wall > 0 else 0.0
    summary = {
        "mode": MODE,
        "conc": CONC,
        "rounds_per_conn": ROUNDS,
        "pool_size": len(word_pool),
        "total_requests": total,
        "wall_seconds": round(wall, 3),
        "qps": round(qps, 1),
        "avg_ms": round(sum(all_lat) / total, 3),
        "p50_ms": round(percentile(all_lat, 50), 3),
        "p95_ms": round(percentile(all_lat, 95), 3),
        "p99_ms": round(percentile(all_lat, 99), 3),
        "max_ms": round(all_lat[-1], 3),
        "errors": len(errors),
    }

    print("=" * 58)
    print("并发=%d  每连接请求=%d  总请求=%d" % (CONC, ROUNDS, total))
    print("耗时 %.2fs   QPS=%.1f" % (wall, qps))
    print("延迟 avg=%.2fms  p50=%.2fms  p95=%.2fms  p99=%.2fms  max=%.2fms"
          % (summary["avg_ms"], summary["p50_ms"], summary["p95_ms"],
             summary["p99_ms"], summary["max_ms"]))
    print("错误数=%d" % len(errors))
    print("=" * 58)
    if errors:
        print("\n".join(errors[:10]))

    if OUT_FILE:
        with open(OUT_FILE, "a", encoding="utf-8") as fh:
            fh.write(json.dumps(summary, ensure_ascii=False) + "\n")
        print("结果已追加到 %s" % OUT_FILE)

    return 0 if not errors else 1


if __name__ == "__main__":
    sys.exit(main())
