#include "database.h"

#include <cstdlib>
#include <exception>
#include <iostream>
#include <string>
#include <vector>

int main(int argc, char* argv[]) {
    if (argc != 2 && argc != 3) {
        std::cerr
            << "Usage: ./minitalk_db_demo <username> [message]\n";
        return 1;
    }

    const char* password = std::getenv("MINITALK_DB_PASSWORD");

    if (password == nullptr) {
        std::cerr << "MINITALK_DB_PASSWORD is not set.\n";
        return 1;
    }

    const std::string username = argv[1];

    try {
        Database database(
            "tcp://127.0.0.1:3306",
            "minitalk_app",
            password,
            "minitalk"
        );

        std::cout << "Connected to MySQL successfully.\n";

        if (argc == 3) {
            database.saveMessage(username, argv[2]);
            std::cout << "Message saved successfully.\n";
        }

        const std::vector<ChatMessage> messages =
            database.getMessagesByUser(username);

        std::cout << "Messages sent by " << username << ":\n";

        for (const ChatMessage& message : messages) {
            std::cout
                << "id=" << message.id
                << ", username=" << message.username
                << ", content=" << message.content
                << ", created_at=" << message.created_at
                << '\n';
        }
    } catch (const std::exception& error) {
        std::cerr << "Database error: "
                  << error.what() << '\n';

        return 1;
    }

    return 0;
}