#include <arpa/inet.h>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <netinet/in.h>
#include <string>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>
#include "unique_fd.h"

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
    if (argc != 3) {
        std::cerr << "Usage: ./minitalk_client <server_ip> <port>\n";
        return 1;
    }

    const int port = std::atoi(argv[2]);

    UniqueFd client_socket{socket(AF_INET, SOCK_STREAM, 0)};

    if (!client_socket.valid()) {
        std::perror("socket");
        return 1;
    }

    const int client_fd = client_socket.get();

    sockaddr_in server_addr {};
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(static_cast<uint16_t>(port));

    if (inet_pton(AF_INET, argv[1], &server_addr.sin_addr) != 1) {
        std::cerr << "Invalid IPv4 address\n";
        return 1;
    }

    if (connect(client_fd,
                reinterpret_cast<sockaddr*>(&server_addr),
                sizeof(server_addr)) == -1) {
        std::perror("connect");
        return 1;
    }

    std::cout << "Connected. Type a message:\n> " << std::flush;

    while (true) {
        fd_set readable_fds;
        FD_ZERO(&readable_fds);

        FD_SET(STDIN_FILENO, &readable_fds);
        FD_SET(client_fd, &readable_fds);

        const int max_fd =
            client_fd > STDIN_FILENO ? client_fd : STDIN_FILENO;

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

        if (FD_ISSET(STDIN_FILENO, &readable_fds)) {
            std::string line;

            if (!std::getline(std::cin, line)) {
                std::cout << "\nInput ended.\n";
                break;
            }

            line += '\n';

            if (!send_all(client_fd, line.data(), line.size())) {
                break;
            }

            std::cout << "> " << std::flush;
        }

        if (FD_ISSET(client_fd, &readable_fds)) {
            char buffer[1024] {};
            const ssize_t received = recv(
                client_fd, buffer, sizeof(buffer), 0
            );

            if (received > 0) {
                std::cout << "\nServer: ";
                std::cout.write(buffer, received);
                std::cout << "> " << std::flush;
            } else if (received == 0) {
                std::cout << "\nServer closed the connection.\n";
                break;
            } else if (errno != EINTR) {
                std::perror("recv");
                break;
            }
        }
    }
    return 0;
}