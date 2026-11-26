#pragma once

#include <condition_variable>
#include <cstddef>
#include <deque>
#include <mutex>
#include <string>
#include <thread>

class RedisWorker {
public:
    RedisWorker(std::string host, int port);
    ~RedisWorker();

    RedisWorker(const RedisWorker&) = delete;
    RedisWorker& operator=(const RedisWorker&) = delete;

    bool enqueueSetPresence(std::string username, int ttl_seconds);
    bool enqueueRemovePresence(std::string username);

private:
    enum class TaskType {
        SetPresence,
        RemovePresence
    };

    struct Task {
        TaskType type;
        std::string username;
        int ttl_seconds = 0;
    };

    void run();

    static constexpr std::size_t kMaxQueueSize = 1000;

    std::string host_;
    int port_;

    std::mutex mutex_;
    std::condition_variable condition_;
    std::deque<Task> tasks_;
    bool stopping_ = false;

    std::thread worker_thread_;
};