# MiniTalk

MiniTalk 是一个基于 Linux Socket 和 C++17 实现的命令行多客户端聊天室。

项目从阻塞式 Echo Server 开始，逐步实现了 `select`、`poll` 和 `epoll LT` 多路复用版本，用于学习 TCP 通信、应用层协议、非阻塞 I/O、事件驱动模型和现代 C++ 资源管理。

## 当前功能

- 基于 TCP 的客户端和服务端通信
- 基于 `poll` 的多客户端聊天室服务端
- 基于 `epoll` LT 的多客户端非阻塞聊天室服务端
- 客户端使用 `select` 同时监听键盘输入和服务端消息
- 多客户端广播聊天
- 昵称修改命令：`/nick <昵称>`
- 在线用户查询命令：`/users`
- 客户端退出命令：`/quit`
- 使用换行符 `\n` 作为应用层消息边界
- 为每个客户端维护独立接收缓冲区，处理 TCP 半包和粘包
- epoll 版本使用非阻塞 socket
- epoll 版本使用 `send_buffer` 和 `EPOLLOUT` 处理部分发送
- 使用 `UniqueFd` 和 RAII 自动管理 socket 文件描述符
- 服务端支持 `Ctrl+C` 和 `SIGTERM` 优雅退出

## 技术栈

- C++17
- Linux Socket API
- TCP/IP
- `select`
- `poll`
- `epoll`
- CMake
- Git

## 项目结构

```text
minitalk/
├── CMakeLists.txt
├── README.md
├── .gitignore
├── src/
│   ├── client.cpp             # 命令行客户端，使用 select 监听输入和服务端消息
│   ├── server.cpp             # poll 多客户端聊天室服务端
│   ├── server_blocking.cpp    # 阻塞式 Echo Server 学习版本
│   ├── server_select.cpp      # select 多客户端 Echo Server 学习版本
│   ├── server_poll.cpp        # poll 聊天室服务端备份版本
│   ├── server_epoll.cpp       # epoll LT 非阻塞聊天室服务端
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

### 运行 poll 版本服务端

```bash
./build/minitalk_server 8888
```

### 运行 epoll LT 版本服务端

```bash
./build/minitalk_epoll_server 8888
```

同一时间只能启动一个服务端版本并监听同一个端口。

### 启动客户端

在其他终端启动多个客户端：

```bash
./build/minitalk_client 127.0.0.1 8888
```

## 使用方式

连接成功后，直接输入文本并按回车即可发送聊天消息：

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
   ├── poll 版本
   │   ├── poll 监听 listen_fd 和全部 client_fd
   │   └── 线性扫描就绪事件
   │
   └── epoll LT 版本
       ├── epoll 监听 listen_fd 和全部 client_fd
       ├── 客户端 socket 使用非阻塞 I/O
       ├── EPOLLIN 读取数据直到 EAGAIN
       ├── 每个客户端维护 recv_buffer
       ├── 完整消息写入所有客户端的 send_buffer
       └── EPOLLOUT 发送数据直到 EAGAIN 或缓冲区清空
```

## 协议设计

MiniTalk 使用基于文本的行协议。

客户端发送的每条消息都以换行符 `\n` 结尾：

```text
/nick alice\n
hello world\n
/users\n
```

服务端不会把一次 `recv()` 直接视为一条完整消息，而是先追加到该客户端的 `recv_buffer`，再按 `\n` 拆分完整消息。因此可以正确处理 TCP 半包和粘包问题。

## epoll 写事件管理

非阻塞 socket 的 `send()` 不保证一次发完所有数据。

MiniTalk 的 epoll 版本为每个客户端维护 `send_buffer`：

```text
聊天消息产生
    ↓
追加到各客户端 send_buffer
    ↓
通过 epoll_ctl 注册 EPOLLOUT
    ↓
socket 可写时，epoll_wait 返回 EPOLLOUT
    ↓
循环 send，直到发送缓冲区清空或返回 EAGAIN
    ↓
发送缓冲区清空后移除 EPOLLOUT
```

仅在 `send_buffer` 非空时关注 `EPOLLOUT`，避免 socket 持续可写时造成 CPU 空转。

## 资源管理

项目中的 `UniqueFd` 负责管理 socket 文件描述符：

- 构造时接管 fd 所有权
- 析构时自动执行 `close(fd)`
- 禁止拷贝，避免同一个 fd 被重复关闭
- 支持移动语义，允许安全转移 fd 所有权

服务端监听 socket、epoll fd 和客户端 socket 均通过 RAII 管理。

## 测试场景

| 场景 | 预期结果 |
|---|---|
| 单个客户端发送消息 | 客户端收到服务端广播消息 |
| 多个客户端保持连接 | 所有客户端即时收到广播消息 |
| 一个客户端退出 | 其他客户端仍可继续聊天 |
| 执行 `/nick alice` | 昵称修改成功并通知其他用户 |
| 两个客户端使用相同昵称 | 服务端拒绝重复昵称 |
| 执行 `/users` | 返回当前在线用户列表 |
| 执行 `/quit` | 当前客户端退出，服务端继续运行 |
| 服务端按 `Ctrl+C` | 服务端退出，客户端感知连接关闭 |
| 长消息发送 | epoll 版本通过 send_buffer 和 EPOLLOUT 继续发送剩余数据 |

## 当前限制

- epoll 版本当前使用 LT 水平触发，尚未实现 ET 边缘触发。
- `send_buffer` 尚未设置最大长度，极慢客户端可能造成内存占用增长。
- 聊天记录和用户信息仅保存在内存中，服务端重启后会丢失。
- 暂未实现用户认证、MySQL、Redis、线程池和 HTTP API。
- 当前项目定位为学习型 Linux C++ 网络编程项目，不宣称具备生产级高并发能力。

## 后续计划

- 为 `send_buffer` 增加长度限制和慢客户端处理策略
- 接入 MySQL 保存用户和聊天记录
- 接入 Redis 保存在线状态或会话信息
- 增加线程池处理耗时业务任务
- 增加日志、配置文件和自动化测试
- 研究 epoll ET 边缘触发模式