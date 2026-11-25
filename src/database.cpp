#include "database.h"

#include <mysql_connection.h>
#include <mysql_driver.h>

#include <cppconn/prepared_statement.h>
#include <cppconn/resultset.h>

#include <memory>
#include <stdexcept>

Database::Database(
    const std::string& url,
    const std::string& username,
    const std::string& password,
    const std::string& schema
) {
    sql::mysql::MySQL_Driver* driver =
        sql::mysql::get_mysql_driver_instance();

    connection_.reset(
        driver->connect(url, username, password)
    );

    connection_->setSchema(schema);
}

Database::~Database() = default;

void Database::saveMessage(
    const std::string& username,
    const std::string& content
) {
    if (content.empty() || content.size() > 1000) {
        throw std::invalid_argument(
            "Message length must be between 1 and 1000."
        );
    }

    std::unique_ptr<sql::PreparedStatement> find_user(
        connection_->prepareStatement(
            "SELECT id FROM users WHERE username = ?"
        )
    );

    find_user->setString(1, username);

    std::unique_ptr<sql::ResultSet> user_result(
        find_user->executeQuery()
    );

    if (!user_result->next()) {
        throw std::runtime_error(
            "User not found: " + username
        );
    }

    const int user_id = user_result->getInt("id");

    std::unique_ptr<sql::PreparedStatement> insert_message(
        connection_->prepareStatement(
            "INSERT INTO messages (sender_id, content) "
            "VALUES (?, ?)"
        )
    );

    insert_message->setInt(1, user_id);
    insert_message->setString(2, content);
    insert_message->executeUpdate();
}

std::vector<ChatMessage> Database::getMessagesByUser(
    const std::string& username
) {
    std::unique_ptr<sql::PreparedStatement> statement(
        connection_->prepareStatement(
            "SELECT "
            "    m.id, "
            "    u.username, "
            "    m.content, "
            "    m.created_at "
            "FROM messages AS m "
            "JOIN users AS u "
            "    ON m.sender_id = u.id "
            "WHERE u.username = ? "
            "ORDER BY m.created_at ASC, m.id ASC"
        )
    );

    statement->setString(1, username);

    std::unique_ptr<sql::ResultSet> result(
        statement->executeQuery()
    );

    std::vector<ChatMessage> messages;

    while (result->next()) {
        messages.push_back(
            ChatMessage{
                result->getInt("id"),
                result->getString("username"),
                result->getString("content"),
                result->getString("created_at")
            }
        );
    }

    return messages;
}