# Network Dictionary 网络电子词典系统

一个基于 **C++17、Qt6 和 TCP Socket** 的 C/S 架构网络电子词典项目。客户端使用 Qt6 Widgets 实现登录注册、单词查询、收藏、发音、主题切换和数据导出；服务端基于 POSIX Socket、线程池、SQLite 和数据库连接池处理并发请求。

> 本项目主要用于 C++ 网络编程、Qt 客户端开发、多线程和数据库连接池的学习与实践。

## 功能特性

- 用户注册、登录、退出、注销和基础参数校验
- 单词查询、搜索历史和单词收藏
- 生词本、单词卡片和单词发音
- 浅色 / 深色主题，通过 QSS 统一样式
- Excel 导出（基于 xlnt）和 PDF 导出（基于 QTextDocument / QPrinter）
- 心跳保活、断线自动重连、消息队列和发送互斥
- 服务端使用线程池处理连接，SQLite 连接池复用数据库连接
- 查词链路配 LRU 热点词缓存，命中率可观测
- 密码以 SHA-256 + 随机盐存储，旧明文账号登录时自动升级

## 技术栈

| 模块 | 技术 |
|---|---|
| 客户端 | C++17、Qt6 Widgets、Qt Network、Qt TextToSpeech、Qt PrintSupport |
| 服务端 | C++17、POSIX TCP Socket、多线程（固定线程池）、SQLite3 |
| 通信协议 | 自定义二进制协议：**v2 变长分帧**（当前），兼容 **v1 定长 1316 字节**；字段统一大端 |
| 数据导出 | xlnt（Excel）、QTextDocument / QPrinter（PDF） |
| 构建工具 | CMake、vcpkg（Windows 客户端依赖）、GitHub Actions |

## 系统架构

```text
┌────────────────────────────────────────┐
│ Qt6 Client                             │
│  UI / PageController                   │
│  MessageDispatcher                     │
│  NetworkManager / ReconnectionManager  │
│  UserSession / WordFunction            │
│  ─ 收发都用 v2 帧（与服务端共用同一个  │
│    shared/protocol_frame.hpp）         │
└───────────────────┬────────────────────┘
                    │ TCP
                    │ v2: magic|len|type|nameLen|textLen|name|text
                    │ v1: 定长 1316B（旧客户端仍可连）
┌───────────────────▼────────────────────┐
│ C++ Server                             │
│  accept 循环（非阻塞监听）             │
│      │                                 │
│      ├─ 每连接一个工作线程 ──► 分帧解析（自动识别 v1/v2）
│      │                          │      │
│      │                          ▼      │
│      │                   processMessage│
│      │                          │      │
│      │        ┌─────────────────┼─────────────────┐
│      │        ▼                 ▼                 ▼
│      │   WordCache(LRU)   DatabaseManager   ThreadPool
│      │   命中即跳过 SQL    表级锁 + 连接池   固定 4 工作者
│      │        │                 │
│      │        └────────► SQLite（WAL 模式）
│      │                          │
│      └─ 心跳超时踢除死连接      ▼
│                            database.db
└────────────────────────────────────────┘
```

服务端启动时会读取当前运行目录下的 `dict.txt`，将词典数据初始化到 SQLite 数据库；运行过程中生成的 `database.db` 属于运行时数据，不纳入 Git。

## 目录结构

```text
network_dictory_project/
├─ .github/workflows/ci.yml     # GitHub Actions：服务端协议测试 + 客户端构建与 CTest
├─ shared/                      # 双端共用的协议定义（v1/v2 帧格式与编解码）
│  └─ protocol_frame.hpp
├─ dicclient/                   # Qt6 客户端
│  ├─ CMakeLists.txt
│  └─ src/
│     ├─ main_app/              # 程序入口
│     ├─ network/               # 网络连接、重连、消息队列
│     ├─ protocol/              # 消息结构、应用配置、协议工具
│     ├─ dispatcher/            # 消息分发
│     ├─ ui/                    # 页面、组件、QSS、资源
│     ├─ user_session/          # 用户会话和本地会话数据
│     ├─ word_function/         # 单词查询、收藏、生词本、导出
│     └─ tests/                 # GoogleTest 单元测试与集成测试（18 个 CTest 用例）
└─ dicserver/                   # C++ 服务端
   ├─ CMakeLists.txt
   ├─ dict.txt                  # 初始化词典数据（约 22 万条）
   ├─ src/
   │  ├─ server.cpp/.hpp        # 收发循环、分帧、各业务响应
   │  ├─ database_manager.*     # SQLite 访问、表级锁、密码哈希校验
   │  ├─ connectionPool.hpp     # 数据库连接池（含 WAL 等 PRAGMA）
   │  ├─ threadPool.*           # 固定大小线程池
   │  ├─ password_hash.hpp      # 自实现 SHA-256 + 随机盐（header-only）
   │  └─ word_cache.hpp         # LRU 热点词缓存（header-only）
   └─ test/                     # 协议级集成测试（Python）+ C++ 单元测试
```

## 通信协议

协议定义放在 `shared/protocol_frame.hpp`，服务端与 Qt 客户端引用**同一份头文件**，避免两端各写一份导致字段宽度或字节序漂移。

### v2：变长分帧（当前版本）

```text
偏移  长度  字段
0     4     magic    = 0x44494354（"DICT"）
4     4     bodyLen  = 8 + nameLen + textLen
8     4     type     消息类型（大端）
12    2     nameLen
14    2     textLen
16    n     name
16+n  m     text
帧总长 = 8 + bodyLen
```

只发实际需要的字节：一次心跳双向只要 53 字节，而 v1 需要 2632 字节。

### v1：定长 1316 字节（仍受支持）

```cpp
struct Msg {
    int type;          // 消息类型（大端）
    char name[32];     // 用户名
    char text[1280];   // 密码、查询单词或响应内容
};
```

### 两个版本如何共存

v1 首 4 字节是 `type`（取值 1~11），v2 首 4 字节是固定 magic `0x44494354`，两者不可能相等。服务端看前 4 字节即可判定版本，**新旧客户端可以连同一个端口**。版本按连接判定（首帧定终身），因此帧里无需再带版本字段。

非法帧（magic 错、`bodyLen` 越界、`bodyLen` 与 `nameLen/textLen` 不符）会直接断开连接——此时缓冲里的字节边界已不可信，跳过续解只会错得更远。

### 消息类型

| type | 含义 | 说明 |
|---|---|---|
| 1 | REGISTER | 注册，响应 `SUCCESS:...` / `ERROR:原因` |
| 2 | LOGIN | 登录 |
| 3 | QUIT | 退出 |
| 4 | SEARCH | 查词，响应七字段（`\|^|` 分隔）或 `NOT_FOUND` |
| 5 | HISTORY | 历史记录，逐条发送后以 `**OVER**` 结束 |
| 6 | HEARTBEAT | 心跳，响应 `ALIVE` |
| 7 | COLLECT | 收藏（toggle） |
| 8 | COLLECT_LIST | 收藏列表 |
| 9 | UNREGISTER | 注销账号，连带删除历史/收藏/生词本 |
| 10 | VOCABULARY | 生词本增删（toggle） |
| 11 | VOCABULARY_LIST | 生词本列表 |

响应统一采用结构化前缀：成功为 `SUCCESS:<字段...>`，失败为 `ERROR:<原因>`，字段间以 `:` 分隔；查词等富文本响应使用 `|^|` 作为词条字段分隔符。

客户端 `app_config.h` 中的服务器地址可用环境变量 `DICT_SERVER_IP` / `DICT_SERVER_PORT` 覆盖，便于本地联调与 CI。

## 服务端关键设计

### 并发模型

`accept` 循环非阻塞接受连接，每个连接交给线程池里的一个工作线程，线程内阻塞 `recv`。这是**每连接一线程**而非 I/O 多路复用——连接数受线程池队列限制，适合本项目当前的客户端规模；若要支撑上万空闲长连接，需要改成 epoll/kqueue + 状态机。

数据库连接池预建连接并复用（`init(db_path, 10, 30)`），避免每个请求反复 `sqlite3_open`。

### 存储：WAL + 表级锁

- 连接建立后执行一次 `PRAGMA journal_mode=WAL; synchronous=NORMAL; busy_timeout=5000`，读不阻塞写，写不阻塞读。
- 原先所有数据库访问共用一把 `db_mutex_`，任何一张表的写都会拖慢全部读。现已按表拆成 `dict_ / usr_ / hist_ / collect_ / vocab_` 等独立锁，查词这类纯读路径不再被收藏、历史写入阻塞。
- 跨表操作（`unregisterUser` 要删四张表）用 `std::lock` 一次性获取多把锁，固定加锁顺序，避免死锁。

### 查词缓存

`dict` 表只在启动时导入、运行期只读，因此查词结果可以安全地缓存在内存里。`word_cache.hpp` 是一个容量可配置的 LRU（`unordered_map` + 双向链表，命中与淘汰都是 O(1)），命中即跳过一次 SQLite 查询和一次锁争用。

只缓存"查得到"的词，避免低频生僻词把热词挤出去。服务端每 5000 次查询输出一次命中率：

```text
[LogLevel::INFO] cache stat: hit=4796 miss=203 size=200/4096 rate=95.94%
```

设置环境变量 `DICT_DISABLE_WORD_CACHE=1` 可以关掉缓存，用于在同一台机器上做 A/B 对比。

### 密码存储

`password_hash.hpp` 是自实现的 SHA-256（不引第三方库），配 16 字节随机盐，库里存 `salt_hex:hash_hex`，校验用恒定时间比较。

存量明文账号不做强制迁移：登录时先按哈希校验，失败再退回明文校验，成功即把密码升级为哈希并清空明文列。哈希与明文都为空时拒绝登录，防止绕过。

## 性能与压测

压测脚本 `dicserver/test/bench_query.py`：多并发长连接持续发查词请求，统计 QPS 与延迟分布。

```bash
cd dicserver/test
DICT_HOST=127.0.0.1 DICT_PORT=8080 DICT_CONC=16 DICT_ROUNDS=300 python3 bench_query.py
```

实测（阿里云 2 核 ECS，16 并发 × 300 请求/连接 = 4800 次查词，服务端线程池 4）：

| 场景 | 缓存 | QPS | avg | p50 | p95 | p99 |
|---|---|---|---|---|---|---|
| 热点集 200 词 | 开 | 2323.7 | 1.40 ms | 1.06 ms | 3.48 ms | 7.45 ms |
| 热点集 200 词 | 关 | 2001.8 | 1.55 ms | 1.24 ms | 3.70 ms | 6.90 ms |
| 词池 7987 词 | 开 | 2068.8 | 1.55 ms | 1.21 ms | 3.63 ms | 7.58 ms |
| 词池 7987 词 | 关 | 1515.1 | 2.00 ms | 1.31 ms | 5.51 ms | 14.00 ms |

热点集场景缓存命中率约 96%，QPS 提升约 16%，平均延迟降约 10%；词池大于缓存容量时命中率降到约 65%，但长尾改善更明显（p99 14.0 ms → 7.6 ms）。

需要说明的是：这是单次实测，同一场景重复跑会有 ±10% 波动；词池远大于缓存容量时，LRU 的维护本身是净开销，收益会随命中率下降而收窄甚至反超——**缓存容量应当按真实热点规模来配**，不是越大越好。

协议层面的收益是确定的：心跳等小报文从 2632 字节（双向各 1316）降到 53 字节，省约 98%。

## 服务端构建与运行

服务端使用 POSIX Socket，建议在 **Linux 或 WSL** 中构建。

### 1. 安装依赖

```bash
sudo apt update
sudo apt install build-essential cmake libsqlite3-dev
```

### 2. 构建

```bash
cd network_dictory_project
cmake -S dicserver -B dicserver/build
cmake --build dicserver/build
```

### 3. 准备运行数据并启动

```bash
cp dicserver/dict.txt dicserver/build/dict.txt
cd dicserver/build
./server 0.0.0.0 8080
```

启动参数依次为服务端监听 IP 和端口。请确保运行目录中存在 `dict.txt`，并开放对应防火墙端口。

## 客户端构建与运行

### 1. 准备依赖

Windows 客户端需要：

- Qt6（包含 Widgets、Network、TextToSpeech、PrintSupport）
- CMake 3.16+
- vcpkg
- xlnt（通过 vcpkg 安装）

```powershell
vcpkg install xlnt
```

### 2. 修改服务器地址

默认地址在：

```text
dicclient/src/protocol/app_config.h
```

两种修改方式：

1. 直接编辑 `app_config.h` 中的 `serverIp` / `serverPort` 默认值；
2. 不改代码，用环境变量覆盖（适合联调与 CI）：

```powershell
$env:DICT_SERVER_IP = "127.0.0.1"
$env:DICT_SERVER_PORT = "8080"
```

### 3. 构建客户端

在项目根目录执行 PowerShell 命令：

```powershell
cmake -S dicclient -B dicclient/build `
  -DCMAKE_TOOLCHAIN_FILE="$env:VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake"
cmake --build dicclient/build --config Release
```

如果 CMake 无法自动找到 Qt，可追加 Qt 安装路径，例如：

```powershell
-DCMAKE_PREFIX_PATH="C:/Qt/6.x.x/msvc2022_64"
```

> 若用 junction 绕开 Ninja 对非 ASCII 路径的问题，CMake 靠相对路径可能找不到仓库根的 `shared/`，可用环境变量 `DICT_SHARED_DIR` 指向它。

构建完成后，Visual Studio 生成器的可执行文件通常位于：

```text
dicclient/build/bin/Release/main_app.exe
```

## 测试与持续集成

项目配备三层自动化测试，由 GitHub Actions 在每次 push / PR 时执行。

### 客户端：GoogleTest + CTest（18 个用例）

每个测试文件编译为独立可执行文件并注册到 CTest，直接链接主工程静态库，覆盖协议编解码、网络重连、UI 组件与登录/注册/主页/注销全流程。集成测试在连不上服务器时按设计 `GTEST_SKIP`，因此本地与 CI 均可稳定运行。

```powershell
# 需要先通过 vcpkg 安装 gtest（gmock 随 gtest 端口提供）与 xlnt
cmake -S dicclient -B dicclient/build -G Ninja -DBUILD_TESTING=ON `
  -DCMAKE_TOOLCHAIN_FILE="$env:VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake" `
  -DCMAKE_PREFIX_PATH="C:/Qt/6.x.x/msvc2022_64"
cmake --build dicclient/build
cd dicclient/build
ctest --output-on-failure
```

### 服务端：C++ 单元测试（CTest）

纯标准库、不依赖 SQLite 与网络，独立可执行：

```bash
cmake -S dicserver -B dicserver/build && cmake --build dicserver/build
cd dicserver/build && ctest --output-on-failure
```

| 用例 | 覆盖内容 |
|---|---|
| `word_cache_test` | LRU 命中/未命中、淘汰顺序、重复写更新、容量上限、0 容量兜底、命中率统计 |
| `protocol_frame_test` | v2 编解码往返、UTF-8 按字节计数、半包、背靠背两帧、版本识别、四类非法帧拒绝 |

### 服务端：协议级集成测试（Python）

```bash
cmake -S dicserver -B dicserver/build && cmake --build dicserver/build
cd dicserver && ./build/server 127.0.0.1 18080 &
DICT_HOST=127.0.0.1 DICT_PORT=18080 python3 test/test_dict.py         # v1：注册/登录/查词/收藏/密码哈希 37 项
DICT_HOST=127.0.0.1 DICT_PORT=18080 python3 test/test_vocabulary.py   # v1：生词本/注销 19 项
DICT_HOST=127.0.0.1 DICT_PORT=18080 python3 test/test_protocol_v2.py  # v2：往返/半包/粘包/非法帧 16 项
DICT_HOST=127.0.0.1 DICT_PORT=18080 python3 test/test_kick.py         # 死连接踢除
```

任一断言失败脚本即以非零码退出，可直接作为 CI 步骤。`test_dict.py` 与 `test_protocol_v2.py` 分别走 v1 与 v2，两者都过才说明新旧协议在同一个端口上共存。

### CI 工作流（.github/workflows/ci.yml）

| 作业 | 平台 | 内容 |
|---|---|---|
| server | ubuntu-latest | 编译服务端，起本地实例，跑三组协议测试 |
| client | windows-latest | 安装 Qt6 与 vcpkg 依赖，构建客户端与全部测试，跑 CTest（`DICT_SERVER_IP=127.0.0.1` 使集成测试确定性跳过，服务可用性由 server 作业验证） |

## 常见问题

1. **客户端连接失败**：检查服务端是否启动、端口是否监听、客户端 IP 是否正确、防火墙是否放行。
2. **提示数据库初始化失败**：确认启动服务端时当前目录存在 `dict.txt`，并且程序有写入权限。
3. **CMake 找不到 Qt**：通过 `CMAKE_PREFIX_PATH` 指定 Qt6 的安装目录。
4. **CMake 找不到 xlnt**：确认已通过 vcpkg 安装，并正确传入 vcpkg 工具链文件。
5. **CMake 报找不到 shared/protocol_frame.hpp**：设置环境变量 `DICT_SHARED_DIR` 指向仓库根的 `shared` 目录（用 junction 构建时相对路径会失效）。
6. **发音功能不可用**：确认系统具备可用的语音引擎，并安装 Qt TextToSpeech 相关组件。

## 说明

本仓库仅包含项目自有源码和初始化词典数据，不包含本地构建目录、vcpkg 依赖目录、编译产物和运行时数据库。项目仅供学习交流使用。
