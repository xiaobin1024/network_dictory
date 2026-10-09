# Network Dictionary 网络电子词典系统

一个基于 **C++17、Qt6 和 TCP Socket** 的 C/S 架构网络电子词典项目。客户端使用 Qt6 Widgets 实现登录注册、单词查询、收藏、发音、主题切换和数据导出；服务端基于 POSIX Socket、线程池、SQLite 和数据库连接池处理并发请求。

> 本项目主要用于 C++ 网络编程、Qt 客户端开发、多线程和数据库连接池的学习与实践。

## 功能特性

- 用户注册、登录、退出和基础参数校验
- 单词查询、搜索历史和单词收藏
- 生词本、单词卡片和单词发音
- 浅色 / 深色主题，通过 QSS 统一样式
- Excel 导出（基于 xlnt）和 PDF 导出（基于 QTextDocument / QPrinter）
- 心跳保活、断线自动重连、消息队列和发送互斥
- 服务端使用线程池处理连接，SQLite 连接池复用数据库连接

## 技术栈

| 模块 | 技术 |
|---|---|
| 客户端 | C++17、Qt6 Widgets、Qt Network、Qt TextToSpeech、Qt PrintSupport |
| 服务端 | C++17、POSIX TCP Socket、多线程、SQLite3 |
| 通信协议 | 自定义定长二进制协议，消息类型字段使用大端字节序 |
| 数据导出 | xlnt（Excel）、QTextDocument / QPrinter（PDF） |
| 构建工具 | CMake、vcpkg（Windows 客户端依赖） |

## 系统架构

```text
┌──────────────────────────────┐
│ Qt6 Client                   │
│ UI / PageController          │
│ MessageDispatcher            │
│ NetworkManager / Reconnect   │
│ UserSession / WordFunction   │
└───────────────┬──────────────┘
                │ TCP Socket + 自定义二进制协议
┌───────────────▼──────────────┐
│ C++ Server                   │
│ TCP Server / Worker Thread   │
│ ThreadPool                   │
│ DatabaseManager              │
│ SQLite ConnectionPool        │
└───────────────┬──────────────┘
                │
                ▼
            SQLite database.db
```

服务端启动时会读取当前运行目录下的 `dict.txt`，将词典数据初始化到 SQLite 数据库；运行过程中生成的 `database.db` 属于运行时数据，不纳入 Git。

## 目录结构

```text
network_dictory_project/
├─ dicclient/                  # Qt6 客户端
│  ├─ CMakeLists.txt
│  └─ src/
│     ├─ main_app/             # 程序入口
│     ├─ network/              # 网络连接、重连、消息队列
│     ├─ protocol/             # 消息结构、应用配置、协议工具
│     ├─ dispatcher/           # 消息分发
│     ├─ ui/                   # 页面、组件、QSS、资源
│     ├─ user_session/         # 用户会话和本地会话数据
│     └─ word_function/        # 单词查询、收藏、生词本、导出
└─ dicserver/                  # C++ 服务端
   ├─ CMakeLists.txt
   ├─ dict.txt                 # 初始化词典数据
   └─ src/
```

## 通信协议

客户端和服务端使用定长二进制消息结构：

```cpp
struct Msg {
    int type;          // 消息类型
    char name[20];     // 用户名
    char text[1280];   // 密码、查询单词或响应内容
};
```

主要消息类型包括注册、登录、退出、查询、历史记录、心跳、收藏和收藏列表。发送前将消息类型转换为大端字节序，接收后再转换回主机字节序。

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

编辑：

```text
dicclient/src/protocol/app_config.h
```

将下面地址改为实际服务器 IP：

```cpp
QString serverIp{"192.168.23.132"};
int serverPort{8080};
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

构建完成后，Visual Studio 生成器的可执行文件通常位于：

```text
dicclient/build/bin/Release/main_app.exe
```

## 常见问题

1. **客户端连接失败**：检查服务端是否启动、端口是否监听、客户端 IP 是否正确、防火墙是否放行。
2. **提示数据库初始化失败**：确认启动服务端时当前目录存在 `dict.txt`，并且程序有写入权限。
3. **CMake 找不到 Qt**：通过 `CMAKE_PREFIX_PATH` 指定 Qt6 的安装目录。
4. **CMake 找不到 xlnt**：确认已通过 vcpkg 安装，并正确传入 vcpkg 工具链文件。
5. **发音功能不可用**：确认系统具备可用的语音引擎，并安装 Qt TextToSpeech 相关组件。

## 说明

本仓库仅包含项目自有源码和初始化词典数据，不包含本地构建目录、vcpkg 依赖目录、编译产物和运行时数据库。项目仅供学习交流使用。
