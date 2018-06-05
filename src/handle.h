#pragma once

#include <functional>
#include <cstring>
#include <memory>
#include <netinet/ip.h>
#include <fcntl.h>

#include "exception.h"

class TEventLoop;
class TSocketHandle;

using TReadHandler = std::function<void(std::shared_ptr<TSocketHandle>)>;
using TWriteHandler = std::function<void(std::shared_ptr<TSocketHandle>)>;
using TAcceptHandler = std::function<void(std::shared_ptr<TSocketHandle>, std::shared_ptr<TSocketHandle>)>;
using TConnectHandler = std::function<void(std::shared_ptr<TSocketHandle>)>;

class TSocketAddress {
public:
    TSocketAddress();
    TSocketAddress(const sockaddr* sa, size_t len);

    inline uint16_t Port() const {
        return Port_;
    }

    inline std::string Host() const {
        return Host_;
    }

    template<class T>
    T* AddressAs() {
        return reinterpret_cast<T*>(&Addr_);
    }

    template<class T>
    T* AddressAs() const {
        return reinterpret_cast<T*>(&Addr_);
    }

    socklen_t Length() const {
        return Len_;
    }

    socklen_t& Length() {
        return Len_;
    }

private:
    sockaddr_storage Addr_;
    socklen_t Len_;
    uint16_t Port_ = 0;
    std::string Host_;
};

std::vector<TSocketAddress> GetAddrInfo(const std::string& host, const std::string& service, bool listener, const std::string& protocol);

class TSocketHandle : public std::enable_shared_from_this<TSocketHandle> {
public:
    void Listen(TAcceptHandler handler, int backlog = 1);
    void Bind(const TSocketAddress& addr);

    ~TSocketHandle();

    TEventLoop* Loop() const {
        return Loop_;
    }

    TReadHandler ReadHandler;
    TWriteHandler WriteHandler;
    TAcceptHandler AcceptHandler;
    TConnectHandler ConnectHandler;

    bool Registered = false;

    int Fd() {
        return Fd_;
    }

    TSocketHandle(TEventLoop* loop, int fd)
        : Loop_(loop)
        , Fd_(fd)
    {
    }

private:
    int Fd_ = -1;
    TEventLoop* Loop_ = nullptr;
};
