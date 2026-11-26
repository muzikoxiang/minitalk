#include "redis_worker.h"

#include "redis_client.h"

#include <exception>
#include <iostream>
#include <utility>

RedisWorker::RedisWorker(std::string host, int port)
    : host_(std::move(host)),
      port_(port),
      worker_thread_(&RedisWorker::run, this) {}

RedisWorker::~RedisWorker() {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        stopping_ = true;
    }

    condition_.notify_one();

    if (worker_thread_.joinable()) {
        worker_thread_.join();
    }
}

bool RedisWorker::enqueueSetPresence(std::string username, int ttl_seconds) {
    {
        std::lock_guard<std::mutex> lock(mutex_);

        if (stopping_ || tasks_.size() >= kMaxQueueSize) {
            return false;
        }

        tasks_.push_back({
            TaskType::SetPresence,
            std::move(username),
            ttl_seconds
        });
    }

    condition_.notify_one();
    return true;
}

bool RedisWorker::enqueueRemovePresence(std::string username) {
    {
        std::lock_guard<std::mutex> lock(mutex_);

        if (stopping_ || tasks_.size() >= kMaxQueueSize) {
            return false;
        }

        tasks_.push_back({
            TaskType::RemovePresence,
            std::move(username),
            0
        });
    }

    condition_.notify_one();
    return true;
}

void RedisWorker::run() {
    try {
        RedisClient redis(host_, port_);

        while (true) {
            Task task;

            {
                std::unique_lock<std::mutex> lock(mutex_);

                condition_.wait(lock, [this] {
                    return stopping_ || !tasks_.empty();
                });

                if (tasks_.empty() && stopping_) {
                    break;
                }

                task = std::move(tasks_.front());
                tasks_.pop_front();
            }

            try {
                if (task.type == TaskType::SetPresence) {
                    redis.setPresence(task.username, task.ttl_seconds);
                    std::cout << "[redis worker] "
                              << task.username << " is online\n";
                } else {
                    redis.removePresence(task.username);
                    std::cout << "[redis worker] "
                              << task.username << " is offline\n";
                }
            } catch (const std::exception& error) {
                std::cerr << "[redis worker] task failed: "
                          << error.what() << '\n';
            }
        }
    } catch (const std::exception& error) {
        std::cerr << "[redis worker] cannot start: "
                  << error.what() << '\n';
    }
}