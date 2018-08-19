#include "reactor.h"
#include "tcp_handle.h"

TTcpHandlePtr TTcpHandle::Create() {
    int fd = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (fd == -1) {
        ThrowErrno("tcp socket creation failed");
    }
    SetNonBlocking(fd);
    return std::make_shared<TTcpHandle>(Reactor(), fd);
}

TResult<TTcpHandlePtr> TTcpHandle::Accept() {
    TSocketAddress addr;
    TResult<int> fd = Reactor()->Accept(Fd(), &addr);
    if (!fd) {
        return TResult<TTcpHandlePtr>::MakeFail(fd.Status());
    }
    auto accepted = std::make_shared<TTcpHandle>(Reactor(), fd.Result());
    accepted->PeerAddress_ = std::move(addr);
    return TResult<TTcpHandlePtr>::MakeSuccess(accepted);
}

TResult<int> TTcpHandle::Connect(const TSocketAddress& addr) {
    return Reactor()->Connect(Fd(), addr);
}

void TTcpHandle::ShutdownAll() {
    if (Active()) {
        ::shutdown(Fd(), SHUT_RDWR);
    }
}

void TTcpHandle::ShutdownRead() {
    if (Active()) {
        ::shutdown(Fd(), SHUT_RD);
    }
}

void TTcpHandle::ShutdownWrite() {
    if (Active()) {
        ::shutdown(Fd(), SHUT_WR);
    }
}

void TTcpHandle::Bind(const TSocketAddress& addr) {
    ASSERT(Active());
    int res = ::bind(Fd(), addr.AddressAs<const sockaddr*>(), addr.Length());
    if (res == -1) {
        ThrowErrno("bind failed");
    }
    SockAdderess_ = addr;
}

void TTcpHandle::Listen(int backlog) {
    ASSERT(Active());
    int res = ::listen(Fd(), backlog);
    if (res == -1) {
        ThrowErrno("listen failed");
    }
}

void TTcpHandle::ReuseAddr() {
    ASSERT(Active());
    int enable = 1;
    int res = setsockopt(Fd(), SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int));
    if (res == -1) {
        ThrowErrno("setsockopt failed");
    }
}

void TTcpHandle::ReusePort() {
    ASSERT(Active());
    int enable = 1;
    int res = setsockopt(Fd(), SOL_SOCKET, SO_REUSEPORT, &enable, sizeof(int));
    if (res == -1) {
        ThrowErrno("setsockopt failed");
    }
}
