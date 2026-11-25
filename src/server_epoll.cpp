#include "unique_fd.h"

#include <cctype>
#include <arpa/inet.h>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <fcntl.h>
#include <iostream>
#include <netinet/in.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <unistd.h>
#include <string>
#include <unordered_map>
#include <utility>

bool set_nonblocking(int fd) {
    const int flags = fcntl(fd, F_GETFL, 0);

    if (flags == -1) {
        std::perror("fcntl F_GETFL");
        return false;
    }

    if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) == -1) {
        std::perror("fcntl F_SETFL");
        return false;
    }

    return true;
}

struct ClientState {
    UniqueFd socket;
    std::string nickname;
    std::string recv_buffer;
    std::string send_buffer;
};

bool update_client_interest(
    int epoll_fd,
    int client_fd,
    const ClientState& client
) {
    epoll_event event {};
    event.events = EPOLLIN | EPOLLRDHUP;

    if (!client.send_buffer.empty()) {
        event.events |= EPOLLOUT;
    }

    event.data.fd = client_fd;

    if (epoll_ctl(epoll_fd, EPOLL_CTL_MOD,
                  client_fd, &event) == -1) {
        std::perror("epoll_ctl modify client_fd");
        return false;
    }

    return true;
}

void broadcast_message(
    int epoll_fd,
    std::unordered_map<int, ClientState>& clients,
    const std::string& message
) {
    for (auto& [fd, client] : clients) {
        client.send_buffer += message;

        update_client_interest(epoll_fd, fd, client);
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
        std::cerr << "Usage: ./minitalk_epoll_server <port>\n";
        return 1;
    }

    const int port = std::atoi(argv[1]);

    UniqueFd listen_socket{socket(AF_INET, SOCK_STREAM, 0)};

    if (!listen_socket.valid()) {
        std::perror("socket");
        return 1;
    }

    int reuse_addr = 1;

    if (setsockopt(listen_socket.get(), SOL_SOCKET, SO_REUSEADDR,
                   &reuse_addr, sizeof(reuse_addr)) == -1) {
        std::perror("setsockopt");
        return 1;
    }

    sockaddr_in server_addr {};
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    server_addr.sin_port = htons(static_cast<uint16_t>(port));

    if (bind(listen_socket.get(),
             reinterpret_cast<sockaddr*>(&server_addr),
             sizeof(server_addr)) == -1) {
        std::perror("bind");
        return 1;
    }

    if (listen(listen_socket.get(), SOMAXCONN) == -1) {
        std::perror("listen");
        return 1;
    }

    if (!set_nonblocking(listen_socket.get())) {
        return 1;
    }

    UniqueFd epoll_fd{epoll_create1(EPOLL_CLOEXEC)};

    if (!epoll_fd.valid()) {
        std::perror("epoll_create1");
        return 1;
    }

    epoll_event listen_event {};
    listen_event.events = EPOLLIN;
    listen_event.data.fd = listen_socket.get();

    if (epoll_ctl(epoll_fd.get(), EPOLL_CTL_ADD,
                  listen_socket.get(), &listen_event) == -1) {
        std::perror("epoll_ctl add listen_fd");
        return 1;
    }

    std::unordered_map<int, ClientState> clients;
    std::cout << "MiniTalk epoll server listening on port "
              << port << '\n';

    epoll_event events[16] {};

    while (true) {
        const int event_count = epoll_wait(
            epoll_fd.get(),
            events,
            16,
            -1
        );

        if (event_count == -1) {
            if (errno == EINTR) {
                continue;
            }

            std::perror("epoll_wait");
            break;
        }

        for (int i = 0; i < event_count; ++i) {
            const int current_fd = events[i].data.fd;
            const uint32_t current_events = events[i].events;

            if (current_fd == listen_socket.get()) {
                while (true) {
                    sockaddr_in client_addr {};
                    socklen_t client_addr_len = sizeof(client_addr);

                    UniqueFd client_socket{
                        accept(
                            listen_socket.get(),
                            reinterpret_cast<sockaddr*>(&client_addr),
                            &client_addr_len
                        )
                    };

                    if (!client_socket.valid()) {
                        if (errno == EAGAIN || errno == EWOULDBLOCK) {
                            break;
                        }

                        if (errno == EINTR) {
                            continue;
                        }

                        std::perror("accept");
                        break;
                    }

                const int client_fd = client_socket.get();

                    if (!set_nonblocking(client_fd)) {
                        continue;
                    }

                    epoll_event client_event {};
                    client_event.events = EPOLLIN | EPOLLRDHUP;
                    client_event.data.fd = client_fd;

                    if (epoll_ctl(epoll_fd.get(), EPOLL_CTL_ADD,
                                client_fd, &client_event) == -1) {
                        std::perror("epoll_ctl add client_fd");
                        continue;
                    }

                    clients.emplace(
                        client_fd,
                        ClientState{
                            std::move(client_socket),
                            "guest" + std::to_string(client_fd),
                            "",
                            ""
                        }
                    );

                    char client_ip[INET_ADDRSTRLEN] {};
                    inet_ntop(AF_INET, &client_addr.sin_addr,
                            client_ip, sizeof(client_ip));

                    std::cout << "Client connected: "
                            << clients.at(client_fd).nickname
                            << " (" << client_ip << ':'
                            << ntohs(client_addr.sin_port)
                            << "), fd = " << client_fd << '\n';
                    continue;
                }
                continue;
            }
            auto client_it = clients.find(current_fd);

            if (client_it == clients.end()) {
                continue;
            }

            bool should_close = false;

            if (current_events & EPOLLIN) {
                while (true) {
                    char buffer[1024] {};

                    const ssize_t received = recv(
                        current_fd, buffer, sizeof(buffer), 0
                    );

                    if (received > 0) {
                        std::string& pending = client_it->second.recv_buffer;

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

                            if (!line.empty() && line.back() == '\r') {
                                line.pop_back();
                            }

                            if (line == "/quit") {
                                broadcast_message(
                                    epoll_fd.get(),
                                    clients,
                                    "* " + client_it->second.nickname +
                                    " left the chat\n"
                                );

                                should_close = true;
                                break;
                            }

                            if (line == "/users") {
                                const std::string users = make_user_list(clients);

                                client_it->second.send_buffer += users;

                                if (!update_client_interest(
                                        epoll_fd.get(),
                                        current_fd,
                                        client_it->second
                                    )) {
                                    should_close = true;
                                    break;
                                }

                                continue;
                            }

                            if (line.rfind("/nick ", 0) == 0) {
                                const std::string new_nickname = line.substr(6);

                                if (!is_valid_nickname(new_nickname)) {
                                    const std::string error =
                                        "System: nickname must contain 1-16 letters, digits, or _\n";

                                    client_it->second.send_buffer += error;

                                    update_client_interest(
                                        epoll_fd.get(),
                                        current_fd,
                                        client_it->second
                                    );

                                    continue;
                                }

                                if (nickname_in_use(clients, current_fd, new_nickname)) {
                                    const std::string error =
                                        "System: nickname is already in use\n";

                                    client_it->second.send_buffer += error;

                                    update_client_interest(
                                        epoll_fd.get(),
                                        current_fd,
                                        client_it->second
                                    );

                                    continue;
                                }

                                const std::string old_nickname = client_it->second.nickname;
                                client_it->second.nickname = new_nickname;

                                broadcast_message(
                                    epoll_fd.get(),
                                    clients,
                                    "* " + old_nickname +
                                    " is now known as " +
                                    new_nickname + '\n'
                                );

                                continue;
                            }

                            const std::string message =
                                "[" + client_it->second.nickname + "] " +
                                line + '\n';

                            broadcast_message(epoll_fd.get(), clients, message);
                        }
                    } else if (received == 0) {
                        should_close = true;
                        break;
                    } else if (errno == EAGAIN || errno == EWOULDBLOCK) {
                        break;
                    } else if (errno == EINTR) {
                        continue;
                    } else {
                        std::perror("recv");
                        should_close = true;
                        break;
                    }
                }
            }
            
            if (current_events & EPOLLOUT) {
                std::string& pending =
                    client_it->second.send_buffer;

                while (!pending.empty()) {
                    const ssize_t sent = send(
                        current_fd,
                        pending.data(),
                        pending.size(),
                        MSG_NOSIGNAL
                    );

                    if (sent > 0) {
                        pending.erase(
                            0,
                            static_cast<std::size_t>(sent)
                        );
                    } else if (sent == -1 &&
                            (errno == EAGAIN || errno == EWOULDBLOCK)) {
                        break;
                    } else if (sent == -1 && errno == EINTR) {
                        continue;
                    } else {
                        std::perror("send");
                        should_close = true;
                        break;
                    }
                }

                if (!should_close &&
                    !update_client_interest(
                        epoll_fd.get(),
                        current_fd,
                        client_it->second
                    )) {
                    should_close = true;
                }
            }

            if (current_events & (EPOLLERR | EPOLLHUP | EPOLLRDHUP)) {
                should_close = true;
            }

            if (should_close) {
                std::cout << "Client disconnected: "
                        << client_it->second.nickname << '\n';

                epoll_ctl(
                    epoll_fd.get(),
                    EPOLL_CTL_DEL,
                    current_fd,
                    nullptr
                );

                clients.erase(current_fd);
            }
        }
    }

    return 0;
}