#include "tcp.h"
#include <coro/reactor.h>

TTcpHandlePtr TTcpHandle::Create(bool ipv6) {
    int fd = ::socket(ipv6 ? AF_INET6 : AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (fd == -1) {
        ThrowErrno("tcp socket creation failed");
    }
    SetNonBlocking(fd);
    return std::make_shared<TTcpHandle>(Reactor(), fd);
}

TResult<TTcpHandlePtr> TTcpHandle::Accept(TReactor::TDeadline deadline) {
    TSocketAddress addr;
    TResult<int> fd = Reactor()->Accept(Fd(), &addr, deadline);
    if (!fd) {
        return TResult<TTcpHandlePtr>::ForwardError(fd);
    }
    auto accepted = std::make_shared<TTcpHandle>(Reactor(), fd.Result());
    accepted->PeerAddress_ = std::move(addr);
    return TResult<TTcpHandlePtr>::MakeSuccess(accepted);
}

TResult<bool> TTcpHandle::Connect(const TSocketAddress& addr, TReactor::TDeadline deadline) {
    return Reactor()->Connect(Fd(), addr, deadline);
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
    const sockaddr* a = addr.AddressAs<const sockaddr*>();
    int res = ::bind(Fd(), a, addr.Length());
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

void TTcpHandle::EnableSockOpt(int level, int optname) {
    int enable = 1;
    int res = setsockopt(Fd(), level, optname, &enable, sizeof(int));
    if (res == -1) {
        ThrowErrno("setsockopt failed");
    }
}

void TTcpHandle::ReuseAddr() {
    EnableSockOpt(SOL_SOCKET, SO_REUSEADDR);
}

void TTcpHandle::ReusePort() {
    EnableSockOpt(SOL_SOCKET, SO_REUSEPORT);
}

void TTcpHandle::Ipv6Only() {
    EnableSockOpt(IPPROTO_IPV6, IPV6_V6ONLY);
}
