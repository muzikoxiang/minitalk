#include "database.h"
#include "database_worker.h"

#include <cstdlib>
#include <exception>
#include <iostream>
#include <string>

int main(int argc, char* argv[]) {
    if (argc != 2 && argc != 3) {
        std::cerr << "Usage: " << argv[0] << " <username> [message]\n";
        return 1;
    }

    const char* password = std::getenv("MINITALK_DB_PASSWORD");
    if (password == nullptr) {
        std::cerr << "MINITALK_DB_PASSWORD is not set\n";
        return 1;
    }

    const std::string url = "tcp://127.0.0.1:3306";
    const std::string db_username = "minitalk_app";
    const std::string db_password = password;
    const std::string schema = "minitalk";

    const std::string username = argv[1];

    try {
        if (argc == 3) {
            {
                DatabaseWorker worker(
                    url, db_username, db_password, schema);

                worker.enqueueSaveMessage(username, argv[2]);

                std::cout << "Save task has been submitted.\n";
            }
            // 离开作用域时，worker 析构：
            // 标记停止 → 等待队列任务处理完 → join 后台线程。
        }

        Database database(url, db_username, db_password, schema);
        const auto messages = database.getMessagesByUser(username);

        std::cout << "\nMessage history for " << username << ":\n";

        for (const auto& message : messages) {
            std::cout << "[" << message.created_at << "] "
                      << message.username << ": "
                      << message.content << '\n';
        }
    } catch (const std::exception& error) {
        std::cerr << "Error: " << error.what() << '\n';
        return 1;
    }

    return 0;
}