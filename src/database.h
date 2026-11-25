#pragma once

#include <memory>
#include <string>
#include <vector>

namespace sql {
class Connection;
}

struct ChatMessage {
    int id;
    std::string username;
    std::string content;
    std::string created_at;
};

class Database {
public:
    Database(
        const std::string& url,
        const std::string& username,
        const std::string& password,
        const std::string& schema
    );

    ~Database();

    Database(const Database&) = delete;
    Database& operator=(const Database&) = delete;

    void saveMessage(
        const std::string& username,
        const std::string& content
    );

    std::vector<ChatMessage> getMessagesByUser(
        const std::string& username
    );

private:
    std::unique_ptr<sql::Connection> connection_;
};