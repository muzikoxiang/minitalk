#include <arpa/inet.h>
#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <netinet/in.h>
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
    if (port <= 0 || port > 65535) {
        std::cerr << "Invalid port: " << argv[1] << '\n';
        return 1;
    }

    const int listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (listen_fd == -1) {
        std::perror("socket");
        return 1;
    }

    int reuse_addr = 1;
    if (setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR,
                   &reuse_addr, sizeof(reuse_addr)) == -1) {
        std::perror("setsockopt");
        close(listen_fd);
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
        close(listen_fd);
        return 1;
    }

    if (listen(listen_fd, SOMAXCONN) == -1) {
        std::perror("listen");
        close(listen_fd);
        return 1;
    }

     std::cout << "Waiting for one client on port " << port << "...\n";

    while (true) {
        sockaddr_in client_addr {};
        socklen_t client_addr_len = sizeof(client_addr);

        const int client_fd = accept(
            listen_fd,
            reinterpret_cast<sockaddr*>(&client_addr),
            &client_addr_len
        );

        if (client_fd == -1) {
            if (errno == EINTR) {
                continue;
            }

            std::perror("accept");
            break;
        }

        char client_ip[INET_ADDRSTRLEN] {};
        inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, sizeof(client_ip));

        std::cout << "Client connected: "
                << client_ip << ':'
                << ntohs(client_addr.sin_port)
                << ", client_fd = " << client_fd << '\n';

        char buffer[1024] {};

        while (true) {
            const ssize_t received = recv(client_fd, buffer, sizeof(buffer), 0);

            if (received > 0) {
                std::cout << "Received " << received << " bytes: ";
                std::cout.write(buffer, received);
                std::cout << '\n';

                if (!send_all(client_fd, buffer,
                            static_cast<std::size_t>(received))) {
                    break;
                }
            } else if (received == 0) {
                std::cout << "Client closed the connection.\n";
                break;
            } else if (errno == EINTR) {
                continue;
            } else {
                std::perror("recv");
                break;
            }
        }

        close(client_fd);
        std::cout << "Client socket closed. Waiting for next client...\n";
    }

    close(listen_fd);
    return 0;
}