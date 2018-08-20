#pragma once

#include "handle.h"

class TTcpHandle;
using TTcpHandlePtr = std::shared_ptr<TTcpHandle>;

class TTcpHandle : public THandle {
public:
    using THandle::THandle;

    static TTcpHandlePtr Create();

    void Bind(const TSocketAddress& addr);
    void Listen(int backlog);

    TResult<TTcpHandlePtr> Accept();
    TResult<bool> Connect(const TSocketAddress& endpoint);

    void ReuseAddr();
    void ReusePort();

    void ShutdownAll();
    void ShutdownRead();
    void ShutdownWrite();

    const TSocketAddress& PeerAddress() const {
        return PeerAddress_;
    }

    const TSocketAddress& SockAddress() const {
        return SockAdderess_;
    }

private:
    TSocketAddress SockAdderess_;
    TSocketAddress PeerAddress_;
};
