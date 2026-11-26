#pragma once

#include <condition_variable>
#include <deque>
#include <mutex>
#include <string>
#include <thread>
#include <cstddef>

class DatabaseWorker {
public:
    DatabaseWorker(std::string url,
                   std::string username,
                   std::string password,
                   std::string schema);
    ~DatabaseWorker();

    DatabaseWorker(const DatabaseWorker&) = delete;
    DatabaseWorker& operator=(const DatabaseWorker&) = delete;

    bool enqueueSaveMessage(std::string username, std::string content);

private:
    struct SaveTask {
        std::string username;
        std::string content;
    };

    void run();

    std::string url_;
    std::string username_;
    std::string password_;
    std::string schema_;

    std::mutex mutex_;
    std::condition_variable condition_;
    std::deque<SaveTask> tasks_;
    bool stopping_ = false;

    static constexpr std::size_t kMaxQueueSize = 1000;
    std::thread worker_thread_;
};