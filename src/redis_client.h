#pragma once

#include <memory>
#include <optional>
#include <string>

struct redisContext;

class RedisClient {
public:
    RedisClient(const std::string& host, int port);
    ~RedisClient();

    RedisClient(const RedisClient&) = delete;
    RedisClient& operator=(const RedisClient&) = delete;

    void setPresence(const std::string& username, int ttl_seconds);
    void removePresence(const std::string& username);
    std::optional<std::string> getPresence(const std::string& username);
    long long getPresenceTtl(const std::string& username);

private:
    struct ContextDeleter {
        void operator()(redisContext* context) const noexcept;
    };

    std::string makePresenceKey(const std::string& username) const;

    std::unique_ptr<redisContext, ContextDeleter> context_;
};