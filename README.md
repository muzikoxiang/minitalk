# MiniTalk

一个基于 Linux / C++17 的命令行多人聊天室项目。

项目从阻塞式 Socket 通信逐步演进到 `poll`、`epoll`，并接入 MySQL 持久化和 Redis 在线状态管理。重点不只是“能聊天”，还包括非阻塞 I/O、事件驱动、RAII、线程任务队列、背压与服务端状态管理。

## 当前功能

- 基于 TCP 的多人聊天室
- `poll` 与 `epoll` 两个服务器版本
- `epoll` 版本使用 Level Triggered（LT）模式
- 非阻塞监听、`accept`、`recv`、`send`
- 基于换行符 `\n` 的应用层消息协议
- 正确处理半包、粘包、部分发送
- `EPOLLOUT` 事件按需订阅，避免空转
- 聊天广播
- 自定义昵称、在线用户列表、退出命令
- `UniqueFd` RAII 封装，自动关闭文件描述符
- MySQL 异步写入聊天消息
- 有界数据库任务队列，避免慢数据库导致内存无限积压
- Redis 记录用户在线状态
- Redis TTL 自动过期与定时续租
- Redis、MySQL 操作均通过后台线程执行，避免直接阻塞 epoll 网络线程

## 项目结构

```text
minitalk/
├── CMakeLists.txt
├── README.md
├── sql/
│   ├── 001_create_users.sql
│   └── 002_create_messages.sql
└── src/
    ├── client.cpp                 # 聊天客户端
    ├── server_blocking.cpp        # 阻塞式服务器学习版本
    ├── server_select.cpp          # select 服务器学习版本
    ├── server.cpp                 # poll 服务器
    ├── server_epoll.cpp           # epoll 主服务器
    ├── unique_fd.h                # 文件描述符 RAII 封装
    ├── database.h / database.cpp  # MySQL 数据访问封装
    ├── database_worker.h/.cpp     # MySQL 后台任务线程
    ├── db_demo.cpp                # MySQL 功能验证程序
    ├── redis_client.h/.cpp        # Redis 客户端封装
    ├── redis_worker.h/.cpp        # Redis 后台任务线程
    └── redis_demo.cpp             # Redis 功能验证程序
```

## 架构

```text
多个客户端
    │
    ▼
epoll 网络线程
    │
    ├── 接收、拆包、命令处理、广播
    │
    ├── DatabaseWorker 任务队列
    │       └── 后台线程 → MySQL
    │
    └── RedisWorker 任务队列
            └── 后台线程 → Redis
```

网络线程只负责网络 I/O 与聊天室内存状态，不直接执行 MySQL 或 Redis 的同步调用，避免慢查询或网络抖动卡住全部客户端。

## 环境要求

- Linux
- CMake >= 3.16
- 支持 C++17 的编译器
- MySQL Server
- Redis Server
- MySQL Connector/C++
- hiredis

Ubuntu / Debian 可安装依赖：

```bash
sudo apt update
sudo apt install build-essential cmake
sudo apt install mysql-server libmysqlcppconn-dev
sudo apt install redis-server libhiredis-dev
```

检查服务：

```bash
sudo systemctl status mysql --no-pager
sudo systemctl status redis-server --no-pager
```

## 初始化数据库

创建数据库和项目用户时，请自行设置强密码，不要把密码提交到仓库。

```sql
CREATE DATABASE minitalk
    CHARACTER SET utf8mb4
    COLLATE utf8mb4_0900_ai_ci;

CREATE USER 'minitalk_app'@'localhost'
    IDENTIFIED BY 'your_strong_password';

GRANT ALL PRIVILEGES ON minitalk.* TO 'minitalk_app'@'localhost';

FLUSH PRIVILEGES;
```

执行建表脚本：

```bash
mysql -u minitalk_app -p minitalk < sql/001_create_users.sql
mysql -u minitalk_app -p minitalk < sql/002_create_messages.sql
```

当前聊天消息会关联 `users` 表中的用户。因此测试前至少插入测试用户：

```sql
INSERT INTO users (username, password_hash)
VALUES
    ('alice', 'development_only_placeholder'),
    ('bob', 'development_only_placeholder');
```

> 当前 `/nick` 只是聊天室昵称，不是正式登录鉴权。密码哈希字段目前仅为后续登录功能预留。

## 构建

```bash
cmake -S . -B build
cmake --build build
```

构建完成后会生成：

```text
build/minitalk_server
build/minitalk_epoll_server
build/minitalk_client
build/minitalk_db_demo
build/minitalk_redis_demo
```

## 运行聊天室

### 1. 启动 epoll 服务器

MySQL 密码只通过环境变量提供，不写入源码、Git 仓库或 README。

```bash
read -s -p "Database password: " MINITALK_DB_PASSWORD
echo
export MINITALK_DB_PASSWORD

./build/minitalk_epoll_server
```

### 2. 启动客户端

另开一个终端：

```bash
cd ~/minitalk
./build/minitalk_client
```

再开其他终端启动更多客户端。

### 3. 聊天命令

```text
/nick alice     修改昵称
/users          查看当前聊天室连接用户
/quit           退出聊天室
```

普通文本会广播给全部客户端。

建议在数据库中已有 `alice`、`bob` 后，分别使用：

```text
/nick alice
/nick bob
```

普通消息会被异步写入 MySQL；昵称设置后，Redis 中会记录对应用户的在线状态。

## 验证 MySQL 消息持久化

```bash
read -s -p "Database password: " MINITALK_DB_PASSWORD
echo
export MINITALK_DB_PASSWORD

./build/minitalk_db_demo alice "Message saved from C++"

unset MINITALK_DB_PASSWORD
```

该程序会写入一条消息并输出该用户的历史消息。

## 验证 Redis 在线状态

昵称修改为 `alice` 后：

```bash
redis-cli GET minitalk:presence:alice
redis-cli TTL minitalk:presence:alice
```

预期输出：

```text
"online"
(integer) 300
```

用户正常退出后：

```bash
redis-cli GET minitalk:presence:alice
```

预期：

```text
(nil)
```

在线用户的 Redis 状态规则：

- `/nick alice`：写入 `minitalk:presence:alice = online`
- TTL 为 300 秒
- epoll 服务器每 60 秒自动续租
- `/quit` 或连接断开：异步删除在线状态
- 即使服务端异常退出，TTL 到期后也会自动清理残留状态

## 实现要点

### 非阻塞与 epoll

- 监听 socket 与客户端 socket 均设置为非阻塞
- `recv()` 持续读取，直到返回 `EAGAIN` / `EWOULDBLOCK`
- `send()` 可能只发送部分数据，剩余数据保存在 `send_buffer`
- 仅当 `send_buffer` 非空时监听 `EPOLLOUT`
- `EPOLLOUT` 到达后继续发送，缓冲区清空后移除 `EPOLLOUT` 兴趣

### 应用层协议

TCP 是字节流协议，不保证一次 `send()` 对应一次 `recv()`。

MiniTalk 使用换行符作为消息边界：

```text
一条消息\n
```

服务端将未处理数据保存在 `recv_buffer`，循环查找 `\n` 并逐条处理，因此能处理：

- 半包
- 粘包
- 一次接收多条消息
- 一条消息分多次接收

### 数据库任务队列与背压

MySQL 操作不在 epoll 线程直接执行：

```text
epoll 网络线程
    │
    └── enqueueSaveMessage(...)
            │
            ▼
      DatabaseWorker 队列
            │
            ▼
      MySQL 后台线程
```

任务队列最大容量为 1000：

- 队列未满：消息正常异步落库
- 队列满或后台线程停止：聊天仍会广播，但本条消息不持久化，并在服务端打印告警

这是“实时通信优先于慢存储”的明确设计取舍。

### Redis 在线状态

Redis 同样由 `RedisWorker` 后台线程处理：

```text
改昵称 → 删除旧昵称状态 → 写入新昵称 online + TTL
断开连接 → 删除在线状态
服务器运行中 → 定时续租 TTL
```

## 已知限制

- 当前没有注册、登录和正式密码校验
- `password_hash` 尚未接入 Argon2 / bcrypt 等安全哈希
- `/nick` 不等于身份认证
- 数据库任务失败目前仅记录日志，不做重试或死信队列
- Redis 在线状态只代表当前聊天室服务进程维护的状态
- 没有 TLS、HTTP API、日志系统、配置文件和监控系统
- 目前为单机学习项目，未实现多进程、多实例部署
- epoll 当前使用 LT 模式，未实现 ET 模式
- MySQL / Redis 连接参数仍有部分写死在源码中，后续应统一迁移到环境变量或配置文件

## 后续计划

- [ ] 使用 Argon2 或 bcrypt 实现安全密码哈希
- [ ] 实现注册、登录与会话管理
- [ ] 通过配置文件或环境变量统一管理服务配置
- [ ] 增加聊天历史查询
- [ ] 增加单元测试与压力测试
- [ ] 增加日志与错误指标
- [ ] 实现 Redis 连接重连策略
- [ ] 研究 epoll ET 模式与更高并发下的设计取舍

## 学习收获

这个项目覆盖了以下 C++ 后端基础能力：

- Linux Socket / TCP 通信
- `select`、`poll`、`epoll`
- 非阻塞 I/O
- Reactor 事件驱动思想
- RAII 与资源管理
- 线程、互斥锁、条件变量
- 生产者消费者队列
- 背压
- MySQL 基础与预处理语句
- Redis TTL、Hash、在线状态
- CMake 与 Git 基本工作流