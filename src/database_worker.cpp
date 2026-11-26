#include "database_worker.h"

#include "database.h"

#include <exception>
#include <iostream>
#include <stdexcept>
#include <utility>

DatabaseWorker::DatabaseWorker(std::string url,
                               std::string username,
                               std::string password,
                               std::string schema)
    : url_(std::move(url)),
      username_(std::move(username)),
      password_(std::move(password)),
      schema_(std::move(schema)),
      worker_thread_(&DatabaseWorker::run, this) {}

DatabaseWorker::~DatabaseWorker() {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        stopping_ = true;
    }

    condition_.notify_one();

    if (worker_thread_.joinable()) {
        worker_thread_.join();
    }
}

void DatabaseWorker::enqueueSaveMessage(std::string username,
                                        std::string content) {
    {
        std::lock_guard<std::mutex> lock(mutex_);

        if (stopping_) {
            throw std::runtime_error("DatabaseWorker is stopping");
        }

        tasks_.push_back({std::move(username), std::move(content)});
    }

    condition_.notify_one();
}

void DatabaseWorker::run() {
    try {
        // 连接只在这个后台线程中创建和使用，不跨线程共享。
        Database database(url_, username_, password_, schema_);

        while (true) {
            SaveTask task;

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
                database.saveMessage(task.username, task.content);
                std::cout << "[db worker] message saved for "
                          << task.username << '\n';
            } catch (const std::exception& error) {
                std::cerr << "[db worker] save failed: "
                          << error.what() << '\n';
            }
        }
    } catch (const std::exception& error) {
        std::cerr << "[db worker] cannot start: "
                  << error.what() << '\n';
    }
}