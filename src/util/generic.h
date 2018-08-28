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
        res.Error_ = static_cast<uint64_t>(status) << 2;
        return res;
    }

    static TResult<T> MakeSuccess(T result) {
        TResult<T> res;
        res.Error_ = 0;
        res.Result_ = std::move(result);
        return res;
    }

    static TResult<T> MakeCanceled() {
        TResult<T> res;
        res.Error_ = 1;
        return res;
    }

    static TResult<T> MakeTimedOut() {
        TResult<T> res;
        res.Error_ = 2;
        return res;
    }

    /*
     * Forwards error from result `other`.
     */
    template <typename TT>
    static TResult<T> ForwardError(TResult<TT> other) {
        TResult<T> res;
        res.Error_ = other.RawError();
        return res;
    }

    int Error() const {
        return Error_ >> 2;
    }

    bool Canceled() const {
        return Error_ & 1;
    }

    bool TimedOut() const {
        return Error_ & 2;
    }

    const T& Result() const {
        return Result_;
    }

    T& Result() {
        return Result_;
    }

    explicit operator bool() const {
        return Error_ == 0;
    }

    uint64_t RawError() const {
        return Error_;
    }

private:
    uint64_t Error_ = -1;
    T Result_;
};

inline std::string ErrorDescription(int err) {
    char error[1024];
    return std::string(strerror_r(err, error, sizeof(error)));
}

#define ThrowErr(err, expr)  \
    char error[1024]; \
    throw (TException() << expr) << ": " << strerror_r(err, error, sizeof(error));

#define ThrowErrno(expr) ThrowErr(errno, expr)
