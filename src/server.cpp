#include <arpa/inet.h>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <netinet/in.h>
#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>
#include <vector>
#include <string>
#include <unordered_map>
#include <cctype>
#include "unique_fd.h"
#include <utility>

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

struct ClientState {
    UniqueFd socket;
    std::string nickname;
    std::string recv_buffer;
};

void broadcast_message(
    const std::vector<pollfd>& poll_fds,
    const std::string& message
) {
    // 下标 0 是 listen_fd，不参与聊天。
    for (std::size_t i = 1; i < poll_fds.size(); ++i) {
        const int client_fd = poll_fds[i].fd;

        if (!send_all(client_fd, message.data(), message.size())) {
            std::cerr << "Failed to send message to fd "
                      << client_fd << '\n';
        }
    }
}

bool is_valid_nickname(const std::string& nickname) {
    if (nickname.empty() || nickname.size() > 16) {
        return false;
    }

    for (unsigned char ch : nickname) {
        if (!std::isalnum(ch) && ch != '_') {
            return false;
        }
    }

    return true;
}

bool nickname_in_use(
    const std::unordered_map<int, ClientState>& clients,
    int current_fd,
    const std::string& nickname
) {
    for (const auto& [fd, client] : clients) {
        if (fd != current_fd && client.nickname == nickname) {
            return true;
        }
    }

    return false;
}

std::string make_user_list(
    const std::unordered_map<int, ClientState>& clients
) {
    std::string result = "System: online users (" +
                         std::to_string(clients.size()) + ")\n";

    for (const auto& [fd, client] : clients) {
        result += " - " + client.nickname + '\n';
    }

    return result;
}

int main(int argc, char* argv[]) {
    if (argc != 2) {
        std::cerr << "Usage: ./minitalk_server <port>\n";
        return 1;
    }

    const int port = std::atoi(argv[1]);

    UniqueFd listen_socket{socket(AF_INET, SOCK_STREAM, 0)};

    if (!listen_socket.valid()) {
        std::perror("socket");
        return 1;
    }

    const int listen_fd = listen_socket.get();

    int reuse_addr = 1;
    if (setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR,
                   &reuse_addr, sizeof(reuse_addr)) == -1) {
        std::perror("setsockopt");
        return 1;
    }

    sockaddr_in server_addr {};
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    server_addr.sin_port = htons(static_cast<uint16_t>(port));

    if (bind(listen_fd,
             reinterpret_cast<sockaddr*>(&server_addr),
             sizeof(server_addr)) == -1) {
        std::perror("bind");
        return 1;
    }

    if (listen(listen_fd, SOMAXCONN) == -1) {
        std::perror("listen");
        return 1;
    }

    std::vector<pollfd> poll_fds;
    std::unordered_map<int, ClientState> clients;
    poll_fds.push_back({listen_fd, POLLIN, 0});

    std::cout << "MiniTalk poll server listening on port "
              << port << '\n';

    while (true) {
        const int ready_count = poll(
            poll_fds.data(),
            static_cast<nfds_t>(poll_fds.size()),
            -1
        );

        if (ready_count == -1) {
            if (errno == EINTR) {
                continue;
            }

            std::perror("poll");
            break;
        }

        for (std::size_t i = 0; i < poll_fds.size();) {
            const short events = poll_fds[i].revents;

            if (events == 0) {
                ++i;
                continue;
            }

            const int current_fd = poll_fds[i].fd;

            if (current_fd == listen_fd) {
                if (events & POLLIN) {
                    sockaddr_in client_addr {};
                    socklen_t client_addr_len = sizeof(client_addr);

                    UniqueFd client_socket{
                        accept(
                            listen_fd,
                            reinterpret_cast<sockaddr*>(&client_addr),
                            &client_addr_len
                        )
                    };

                    if (!client_socket.valid()) {
                        std::perror("accept");
                    } else {
                        const int client_fd = client_socket.get();

                        char client_ip[INET_ADDRSTRLEN] {};
                        inet_ntop(AF_INET, &client_addr.sin_addr,
                                client_ip, sizeof(client_ip));

                        clients.emplace(
                            client_fd,
                            ClientState{
                                std::move(client_socket),
                                "guest" + std::to_string(client_fd),
                                ""
                            }
                        );

                        poll_fds.push_back({client_fd, POLLIN, 0});

                        std::cout << "Client connected: "
                                << client_ip << ':'
                                << ntohs(client_addr.sin_port)
                                << ", fd = " << client_fd << '\n';

                        broadcast_message(
                            poll_fds,
                            "* " + clients.at(client_fd).nickname + " joined the chat\n"
                        );
                    }
                }

                ++i;
                continue;
            }

            bool should_close = false;

            if (events & POLLIN) {
                char buffer[1024] {};
                const ssize_t received = recv(
                    current_fd, buffer, sizeof(buffer), 0
                );

                if (received > 0) {
                    std::string& pending = clients.at(current_fd).recv_buffer;

                    pending.append(
                        buffer,
                        static_cast<std::size_t>(received)
                    );

                    while (true) {
                        const std::size_t newline_pos = pending.find('\n');

                        if (newline_pos == std::string::npos) {
                            break;
                        }

                        std::string line = pending.substr(0, newline_pos);
                        pending.erase(0, newline_pos + 1);

                        // 兼容 Windows 风格的 \r\n。
                        if (!line.empty() && line.back() == '\r') {
                            line.pop_back();
                        }

                        if (line.empty()) {
                            continue;
                        }

                       if (line == "/quit") {
                            broadcast_message(
                                poll_fds,
                                "* " + clients.at(current_fd).nickname + " left the chat\n"
                            );

                            should_close = true;
                            break;
                        }

                        if (line == "/users") {
                            const std::string users = make_user_list(clients);
                            send_all(current_fd, users.data(), users.size());
                            continue;
                        }

                        if (line.rfind("/nick ", 0) == 0) {
                            const std::string new_nickname = line.substr(6);

                            if (!is_valid_nickname(new_nickname)) {
                                const std::string error =
                                    "System: nickname must contain 1-16 letters, digits, or _\n";

                                send_all(current_fd, error.data(), error.size());
                                continue;
                            }

                            if (nickname_in_use(clients, current_fd, new_nickname)) {
                                const std::string error =
                                    "System: nickname is already in use\n";

                                send_all(current_fd, error.data(), error.size());
                                continue;
                            }

                            const std::string old_nickname = clients.at(current_fd).nickname;
                            clients.at(current_fd).nickname = new_nickname;

                            broadcast_message(
                                poll_fds,
                                "* " + old_nickname + " is now known as " +
                                new_nickname + '\n'
                            );

                            continue;
                        }

                        const std::string message =
                            "[" + clients.at(current_fd).nickname + "] " + line + '\n';

                        broadcast_message(poll_fds, message);
                    }
                } else if (received == 0) {
                    std::cout << "Client fd "
                              << current_fd << " disconnected.\n";
                    should_close = true;
                } else if (errno != EINTR) {
                    std::perror("recv");
                    should_close = true;
                }
            }

            if (events & (POLLERR | POLLHUP | POLLNVAL)) {
                should_close = true;
            }

            if (should_close) {
                poll_fds.erase(
                    poll_fds.begin() + static_cast<std::ptrdiff_t>(i)
                );
                
                clients.erase(current_fd);
            } else {
                ++i;
            }
        }
    }
    return 0;
}