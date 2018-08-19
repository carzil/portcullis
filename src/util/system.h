#pragma once

static constexpr size_t InvalidFd = -1;

inline void SetNonBlocking(int fd) {
    int flags = fcntl(fd, F_GETFL);
    if (flags == -1) {
        char error[4096];
        throw TException() << "fcntl failed: " << strerror_r(errno, error, sizeof(error));
    }
    flags |= O_NONBLOCK;
    if (fcntl(fd, F_SETFL, flags) == -1) {
        char error[4096];
        throw TException() << "fcntl failed: " << strerror_r(errno, error, sizeof(error));
    }
}

inline int64_t GetCurrentMillis() {
    timespec ts;
    if (clock_gettime(CLOCK_MONOTONIC, &ts) == -1) {
        char error[4096];
        throw TException() << "clock_gettime failed: " << strerror_r(errno, error, sizeof(error));
    }
    return ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
}

std::string AbsPath(const std::string& path);
