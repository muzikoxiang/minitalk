#include <mysql_connection.h>
#include <mysql_driver.h>

#include <cppconn/prepared_statement.h>
#include <cppconn/exception.h>
#include <cppconn/resultset.h>
#include <cppconn/statement.h>

#include <cstdlib>
#include <iostream>
#include <memory>

int main(int argc, char* argv[]) {
    if (argc != 2 && argc != 3) {
        std::cerr
            << "Usage: ./minitalk_db_demo <username> [message]\n";
        return 1;
    }

    const std::string username = argv[1];
    const char* password = std::getenv("MINITALK_DB_PASSWORD");

    if (password == nullptr) {
        std::cerr << "MINITALK_DB_PASSWORD is not set.\n";
        return 1;
    }

    try {
        sql::mysql::MySQL_Driver* driver =
            sql::mysql::get_mysql_driver_instance();

        std::unique_ptr<sql::Connection> connection(
            driver->connect(
                "tcp://127.0.0.1:3306",
                "minitalk_app",
                password
            )
        );

        connection->setSchema("minitalk");

        if (argc == 3) {
            const std::string message = argv[2];

            if (message.empty() || message.size() > 1000) {
                std::cerr << "Message length must be between 1 and 1000.\n";
                return 1;
            }

            std::unique_ptr<sql::PreparedStatement> find_user(
                connection->prepareStatement(
                    "SELECT id FROM users WHERE username = ?"
                )
            );

            find_user->setString(1, username);

            std::unique_ptr<sql::ResultSet> user_result(
                find_user->executeQuery()
            );

            if (!user_result->next()) {
                std::cerr << "User not found: " << username << '\n';
                return 1;
            }

            const int user_id = user_result->getInt("id");

            std::unique_ptr<sql::PreparedStatement> insert_message(
                connection->prepareStatement(
                    "INSERT INTO messages (sender_id, content) "
                    "VALUES (?, ?)"
                )
            );

            insert_message->setInt(1, user_id);
            insert_message->setString(2, message);

            insert_message->executeUpdate();

            std::cout << "Message saved successfully.\n";
        }

        std::unique_ptr<sql::PreparedStatement> statement(
            connection->prepareStatement(
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

        std::cout << "Connected to MySQL successfully.\n";

        std::cout << "Messages sent by " << username << ":\n";

        while (result->next()) {
            std::cout
                << "id=" << result->getInt("id")
                << ", username=" << result->getString("username")
                << ", content=" << result->getString("content")
                << ", created_at=" << result->getString("created_at")
                << '\n';
        }
    } catch (const sql::SQLException& error) {
        std::cerr
            << "MySQL error: " << error.what()
            << "\nError code: " << error.getErrorCode()
            << "\nSQL state: " << error.getSQLState()
            << '\n';

        return 1;
    }

    return 0;
}