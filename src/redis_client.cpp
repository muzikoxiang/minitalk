#include "redis_client.h"

#include <hiredis/hiredis.h>

#include <memory>
#include <stdexcept>
#include <vector>

namespace {

using ReplyPtr = std::unique_ptr<redisReply, decltype(&freeReplyObject)>;

ReplyPtr execute(redisContext* context,
                 const std::vector<std::string>& arguments) {
    std::vector<const char*> argv;
    std::vector<std::size_t> argv_lengths;

    for (const auto& argument : arguments) {
        argv.push_back(argument.data());
        argv_lengths.push_back(argument.size());
    }

    auto* raw_reply = static_cast<redisReply*>(
        redisCommandArgv(
            context,
            static_cast<int>(argv.size()),
            argv.data(),
            argv_lengths.data()
        )
    );

    if (raw_reply == nullptr) {
        throw std::runtime_error("Redis command returned no reply");
    }

    return ReplyPtr(raw_reply, freeReplyObject);
}

}  // namespace

void RedisClient::ContextDeleter::operator()(
    redisContext* context
) const noexcept {
    if (context != nullptr) {
        redisFree(context);
    }
}

RedisClient::RedisClient(const std::string& host, int port)
    : context_(redisConnect(host.c_str(), port)) {
    if (!context_ || context_->err != 0) {
        const std::string error =
            context_ ? context_->errstr : "cannot create Redis context";

        throw std::runtime_error("Redis connection failed: " + error);
    }
}

RedisClient::~RedisClient() = default;

void RedisClient::setPresence(const std::string& username, int ttl_seconds) {
    if (username.empty() || ttl_seconds <= 0) {
        throw std::invalid_argument("Invalid Redis presence arguments");
    }

    const auto reply = execute(
        context_.get(),
        {"SETEX", makePresenceKey(username), std::to_string(ttl_seconds), "online"}
    );

    if (reply->type == REDIS_REPLY_ERROR) {
        throw std::runtime_error("Redis SETEX failed: " +
                                 std::string(reply->str, reply->len));
    }
}

void RedisClient::removePresence(const std::string& username) {
    if (username.empty()) {
        throw std::invalid_argument("Username cannot be empty");
    }

    const auto reply = execute(
        context_.get(),
        {"DEL", makePresenceKey(username)}
    );

    if (reply->type == REDIS_REPLY_ERROR) {
        throw std::runtime_error("Redis DEL failed: " +
                                 std::string(reply->str, reply->len));
    }

    if (reply->type != REDIS_REPLY_INTEGER) {
        throw std::runtime_error("Redis DEL returned an unexpected reply");
    }
}

std::optional<std::string> RedisClient::getPresence(
    const std::string& username
) {
    const auto reply = execute(
        context_.get(),
        {"GET", makePresenceKey(username)}
    );

    if (reply->type == REDIS_REPLY_NIL) {
        return std::nullopt;
    }

    if (reply->type == REDIS_REPLY_ERROR) {
        throw std::runtime_error("Redis GET failed: " +
                                 std::string(reply->str, reply->len));
    }

    if (reply->type != REDIS_REPLY_STRING) {
        throw std::runtime_error("Redis GET returned an unexpected reply");
    }

    return std::string(reply->str, reply->len);
}

long long RedisClient::getPresenceTtl(const std::string& username) {
    const auto reply = execute(
        context_.get(),
        {"TTL", makePresenceKey(username)}
    );

    if (reply->type != REDIS_REPLY_INTEGER) {
        throw std::runtime_error("Redis TTL returned an unexpected reply");
    }

    return reply->integer;
}

std::string RedisClient::makePresenceKey(
    const std::string& username
) const {
    return "minitalk:presence:" + username;
}