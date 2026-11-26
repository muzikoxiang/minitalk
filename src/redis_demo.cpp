#include "redis_client.h"
#include "redis_worker.h"

#include <exception>
#include <iostream>

int main() {
    try {
        {
            RedisWorker worker("127.0.0.1", 6379);

            if (!worker.enqueueSetPresence("alice", 60)) {
                std::cerr << "Failed to submit online task\n";
                return 1;
            }

            std::cout << "Online task submitted.\n";
        }
        // worker 析构：等待队列任务完成，再停止并回收后台线程。

        RedisClient redis("127.0.0.1", 6379);

        const auto presence = redis.getPresence("alice");
        const auto ttl = redis.getPresenceTtl("alice");

        std::cout << "alice is "
                  << (presence ? *presence : "offline") << '\n';
        std::cout << "TTL = " << ttl << " seconds\n";

        {
            RedisWorker worker("127.0.0.1", 6379);

            if (!worker.enqueueRemovePresence("alice")) {
                std::cerr << "Failed to submit offline task\n";
                return 1;
            }

            std::cout << "Offline task submitted.\n";
        }

        const auto after_remove = redis.getPresence("alice");

        std::cout << "After logout, alice is "
                  << (after_remove ? *after_remove : "offline") << '\n';
    } catch (const std::exception& error) {
        std::cerr << "Redis error: " << error.what() << '\n';
        return 1;
    }

    return 0;
}