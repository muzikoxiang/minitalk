# MiniTalk

一个基于 Linux Socket 和 C++17 实现的命令行多客户端聊天室。

## 当前功能

- TCP 客户端与服务端通信
- poll 管理多个客户端连接
- 广播聊天消息
- 基于换行符的应用层消息协议
- 昵称命令：`/nick <name>`
- 退出命令：`/quit`
- 客户端使用 select 同时监听键盘输入和服务端消息

## 技术栈

- C++17
- Linux Socket API
- poll 和 select
- CMake

## 构建

```bash
cmake -S . -B build
cmake --build build

运行
启动服务端：
./build/minitalk_server 8888
在其他终端启动多个客户端：
./build/minitalk_client 127.0.0.1 8888

使用方式
/nick alice     修改昵称
/quit           退出聊天室
hello           发送聊天消息

协议
客户端每发送一条消息，都以换行符 \n 作为结束标记。服务端通过每个连接独立的接收缓冲区处理半包和粘包。