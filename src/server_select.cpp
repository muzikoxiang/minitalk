#include <arpa/inet.h>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <netinet/in.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>

bool send_all(int fd, const char* data, std::size_t length) {
    std::size_t sent = 0;

    while (sent < length) {
        const ssize_t result = send(
            fd, data + sent, length - sent, MSG_NOSIGNAL
        );

        if (result > 0) {
            sent += static_cast<std::size_t>(result);
        } else if (result == -1 && errno == EINTR) {
            continue;
        } else {
            std::perror("send");
            return false;
        }
    }

    return true;
}

int main(int argc, char* argv[]) {
    if (argc != 2) {
        std::cerr << "Usage: ./minitalk_server <port>\n";
        return 1;
    }

    const int port = std::atoi(argv[1]);

    const int listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (listen_fd == -1) {
        std::perror("socket");
        return 1;
    }

    int reuse_addr = 1;
    setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR,
               &reuse_addr, sizeof(reuse_addr));

    sockaddr_in server_addr {};
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    server_addr.sin_port = htons(static_cast<uint16_t>(port));

    if (bind(listen_fd,
             reinterpret_cast<sockaddr*>(&server_addr),
             sizeof(server_addr)) == -1) {
        std::perror("bind");
        close(listen_fd);
        return 1;
    }

    if (listen(listen_fd, SOMAXCONN) == -1) {
        std::perror("listen");
        close(listen_fd);
        return 1;
    }

    fd_set all_fds;
    FD_ZERO(&all_fds);
    FD_SET(listen_fd, &all_fds);

    int max_fd = listen_fd;

    std::cout << "MiniTalk select server listening on port "
              << port << '\n';

    while (true) {
        // select 会修改传入集合，所以每轮都从总集合复制。
        fd_set readable_fds = all_fds;

        const int ready_count = select(
            max_fd + 1,
            &readable_fds,
            nullptr,
            nullptr,
            nullptr
        );

        if (ready_count == -1) {
            if (errno == EINTR) {
                continue;
            }

            std::perror("select");
            break;
        }

        // 监听 fd 可读：有新连接到来。
        if (FD_ISSET(listen_fd, &readable_fds)) {
            sockaddr_in client_addr {};
            socklen_t client_addr_len = sizeof(client_addr);

            const int client_fd = accept(
                listen_fd,
                reinterpret_cast<sockaddr*>(&client_addr),
                &client_addr_len
            );

            if (client_fd == -1) {
                std::perror("accept");
            } else if (client_fd >= FD_SETSIZE) {
                std::cerr << "Too many clients for select.\n";
                close(client_fd);
            } else {
                char client_ip[INET_ADDRSTRLEN] {};
                inet_ntop(AF_INET, &client_addr.sin_addr,
                          client_ip, sizeof(client_ip));

                std::cout << "Client connected: "
                          << client_ip << ':'
                          << ntohs(client_addr.sin_port)
                          << ", fd = " << client_fd << '\n';

                FD_SET(client_fd, &all_fds);

                if (client_fd > max_fd) {
                    max_fd = client_fd;
                }
            }
        }

        // 客户端 fd 可读：读取并回显。
        for (int fd = listen_fd + 1; fd <= max_fd; ++fd) {
            if (!FD_ISSET(fd, &readable_fds)) {
                continue;
            }

            char buffer[1024] {};
            const ssize_t received = recv(fd, buffer, sizeof(buffer), 0);

            if (received > 0) {
                std::cout << "Received from fd " << fd << ": ";
                std::cout.write(buffer, received);
                std::cout << '\n';

                if (!send_all(fd, buffer,
                              static_cast<std::size_t>(received))) {
                    FD_CLR(fd, &all_fds);
                    close(fd);
                }
            } else if (received == 0) {
                std::cout << "Client fd " << fd << " disconnected.\n";
                FD_CLR(fd, &all_fds);
                close(fd);
            } else if (errno != EINTR) {
                std::perror("recv");
                FD_CLR(fd, &all_fds);
                close(fd);
            }
        }
    }

    close(listen_fd);
    return 0;
}