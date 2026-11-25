#pragma once

#include <unistd.h>

#include <utility>

class UniqueFd {
public:
    explicit UniqueFd(int fd = -1) noexcept
        : fd_(fd) {}

    ~UniqueFd() {
        reset();
    }

    UniqueFd(const UniqueFd&) = delete;
    UniqueFd& operator=(const UniqueFd&) = delete;

    UniqueFd(UniqueFd&& other) noexcept
        : fd_(std::exchange(other.fd_, -1)) {}

    UniqueFd& operator=(UniqueFd&& other) noexcept {
        if (this != &other) {
            reset(std::exchange(other.fd_, -1));
        }

        return *this;
    }

    int get() const noexcept {
        return fd_;
    }

    bool valid() const noexcept {
        return fd_ != -1;
    }

    void reset(int new_fd = -1) noexcept {
        if (fd_ != -1) {
            close(fd_);
        }

        fd_ = new_fd;
    }

private:
    int fd_;
};