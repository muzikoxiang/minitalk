# MiniTalk

MiniTalk 是一个基于 Linux Socket 和 C++17 实现的命令行多客户端聊天室。

项目从阻塞式 Echo Server 逐步演进到基于 `poll` 的多客户端广播聊天室，用于学习 TCP 通信、I/O 多路复用、应用层协议、Linux 文件描述符管理和现代 C++ RAII。

## 当前功能

- 基于 TCP 的客户端和服务端通信
- 服务端使用 `poll` 同时管理多个客户端连接
- 客户端使用 `select` 同时监听键盘输入和服务端消息
- 多客户端广播聊天
- 昵称修改命令：`/nick <昵称>`
- 在线用户查询命令：`/users`
- 客户端退出命令：`/quit`
- 使用换行符 `\n` 作为一条应用层消息的结束标记
- 每个客户端独立维护接收缓冲区，处理 TCP 半包和粘包
- `UniqueFd` 使用 RAII 自动管理 socket 文件描述符
- 服务端支持 `Ctrl+C` 和 `SIGTERM` 优雅退出

## 技术栈

- C++17
- Linux Socket API
- TCP/IP
- `poll`
- `select`
- CMake
- Git

## 项目结构

```text
minitalk/
├── CMakeLists.txt
├── README.md
├── .gitignore
├── src/
│   ├── client.cpp             # 命令行客户端
│   ├── server.cpp             # 当前 poll 版聊天室服务端
│   ├── server_blocking.cpp    # 阻塞式 Echo Server 学习版本
│   ├── server_select.cpp      # select 多客户端 Echo Server 学习版本
│   └── unique_fd.h            # RAII 文件描述符管理类
└── tests/
```

## 构建环境

- Linux
- 支持 C++17 的编译器，例如 g++
- CMake 3.16 或更高版本

## 编译

在项目根目录执行：

```bash
cmake -S . -B build
cmake --build build
```

## 运行

启动服务端：

```bash
./build/minitalk_server 8888
```

在其他终端启动多个客户端：

```bash
./build/minitalk_client 127.0.0.1 8888
```

## 使用方式

连接成功后，直接输入文本并按回车即可发送聊天消息。

```text
hello
```

支持的命令：

```text
/nick alice    修改昵称为 alice
/users         查看当前在线用户
/quit          退出聊天室
```

## 架构说明

```text
标准输入
   │
   ▼
MiniTalk Client
   ├── select 同时监听 stdin 和服务端 socket
   ├── stdin 可读时，发送用户输入
   └── socket 可读时，立即显示服务端广播消息
   │
   ▼ TCP
   │
MiniTalk Server
   ├── poll 同时监听 listen_fd 和所有 client_fd
   ├── listen_fd 可读时，accept 新客户端
   ├── client_fd 可读时，读取并解析消息
   ├── ClientState 保存昵称和未处理接收缓冲区
   └── 将完整聊天消息广播给所有在线客户端
```

## 协议设计

MiniTalk 使用基于文本的行协议。

客户端发送的每条消息都以换行符 `\n` 结尾：

```text
/nick alice\n
hello world\n
/users\n
```

服务端不会把一次 `recv()` 直接视为一条完整消息，而是先追加到该客户端的接收缓冲区中，再按 `\n` 拆分出完整消息。这使服务端能够处理 TCP 的半包和粘包问题。

## 资源管理

项目中的 `UniqueFd` 负责管理 socket 文件描述符：

- 构造时接管 fd 所有权
- 析构时自动执行 `close(fd)`
- 禁止拷贝，避免同一个 fd 被重复关闭
- 支持移动语义，允许安全转移 fd 所有权

服务端的监听 socket 和每个客户端 socket 都通过 RAII 管理。

## 测试场景

| 场景 | 预期结果 |
|---|---|
| 单个客户端发送消息 | 客户端收到服务端广播的消息 |
| 多个客户端保持连接 | 所有客户端可即时收到广播消息 |
| 一个客户端退出 | 其他客户端仍可继续聊天 |
| 执行 `/nick alice` | 昵称修改成功并通知其他用户 |
| 两个客户端使用相同昵称 | 服务端拒绝重复昵称 |
| 执行 `/users` | 返回当前在线用户列表 |
| 执行 `/quit` | 当前客户端退出，服务端继续运行 |
| 服务端按 `Ctrl+C` | 服务端优雅退出，客户端感知连接关闭 |

## 当前限制

- 当前 socket 仍使用阻塞 I/O。
- `poll` 每次事件循环都需要线性扫描所有 fd。
- 广播发送仍可能因慢客户端而阻塞。
- 暂未实现用户认证、聊天记录持久化、MySQL、Redis、线程池和 HTTP API。
- 当前项目定位为学习型 Linux C++ 网络编程项目，不宣称具备生产级高并发能力。

## 后续计划

- 使用线程池处理耗时业务任务
- 接入 MySQL 保存用户与聊天记录
- 接入 Redis 保存在线状态或会话信息
- 使用 `epoll` 替代 `poll`
- 增加日志、配置文件和自动化测试