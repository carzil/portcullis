#pragma once

#include <unistd.h>
#include <time.h>
#include <fcntl.h>
#include <algorithm>

#include <netdb.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <netinet/ip.h>

#include <cstring>

#include <util/exception.h>

class TMoveOnly {
protected:
    TMoveOnly(const TMoveOnly&) = delete;
    TMoveOnly& operator=(const TMoveOnly&) = delete;

    TMoveOnly(TMoveOnly&&) noexcept = default;
    TMoveOnly& operator=(TMoveOnly&&) noexcept = default;

    TMoveOnly() = default;
    ~TMoveOnly() = default;
};

template <typename T>
class TResult {
public:
    static TResult<T> MakeFail(int status) {
        TResult<T> res;
        res.Error_ = status;
        return res;
    }

    static TResult<T> MakeSuccess(T result) {
        TResult<T> res;
        res.Error_ = 0;
        res.Result_ = std::move(result);
        return res;
    }


    int Error() const {
        return Error_;
    }

    const T& Result() const {
        return Result_;
    }

    T& Result() {
        return Result_;
    }

    explicit operator bool() {
        return Error_ == 0;
    }

private:
    int Error_ = -1;
    T Result_;
};

#define ThrowErr(err, expr)  \
    char error[1024]; \
    throw (TException() << expr) << ": " << strerror_r(err, error, sizeof(error));

#define ThrowErrno(expr) ThrowErr(errno, expr)
