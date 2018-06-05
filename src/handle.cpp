#include <sys/types.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <netinet/ip.h>
#include <sys/socket.h>
#include <netdb.h>

#include <vector>

#include "handle.h"
#include "loop.h"

TSocketAddress::TSocketAddress()
    : Len_(sizeof(sockaddr_storage))
{
}

TSocketAddress::TSocketAddress(const sockaddr* sa, size_t len)
    : Len_(len)
{
    memcpy(&Addr_, sa, len);

    char hostStr[std::max(INET_ADDRSTRLEN, INET6_ADDRSTRLEN)];
    switch (sa->sa_family) {
        case AF_INET:
            Port_ = ::ntohs(reinterpret_cast<const sockaddr_in*>(sa)->sin_port);
            break;
        case AF_INET6:
            Port_ = ::ntohs(reinterpret_cast<const sockaddr_in6*>(sa)->sin6_port);
            break;
        default:
            throw TException() << "unknown socket familly: " << sa->sa_family;
    };
    ::inet_ntop(sa->sa_family, sa, reinterpret_cast<char*>(&hostStr), sizeof(hostStr));
    Host_ = hostStr;
}


TSocketHandle::~TSocketHandle() {
    if (Fd_ != -1) {
        ::close(Fd_);
    }
}

void TSocketHandle::Listen(TAcceptHandler handler, int backlog) {
    ASSERT(!AcceptHandler);
    AcceptHandler = std::move(handler);
    Loop_->Listen(shared_from_this(), backlog);
}

void TSocketHandle::Bind(const TSocketAddress& addr) {
    int res = ::bind(Fd_, addr.AddressAs<const sockaddr>(), addr.Length());
    if (res < 0) {
        char error[4096];
        throw TException() << "bind failed: " << strerror_r(errno, error, sizeof(error));
    }
}

std::vector<TSocketAddress> GetAddrInfo(const std::string& host, const std::string& service, bool listener, const std::string& protocol) {
    int ai_flags = (listener) ? AI_PASSIVE : 0;
    int ai_protocol = 0;
    int ai_family = 0;
    int ai_socktype = 0;

    if (protocol == "tcp") {
        ai_family = AF_INET;
        ai_protocol = IPPROTO_TCP;
        ai_socktype = SOCK_STREAM;
    } else if (protocol == "udp") {
        ai_family = AF_INET;
        ai_protocol = IPPROTO_UDP;
        ai_socktype = SOCK_DGRAM;
    } else {
        throw TException() << "unkonwn protocol '" << protocol << "'";
    }

    struct addrinfo* result;
    struct addrinfo hints;

    hints.ai_flags = ai_flags;
    hints.ai_family = ai_family;
    hints.ai_protocol = ai_protocol;
    hints.ai_socktype = ai_socktype;

    const char* hostPtr = nullptr;
    if (host.size()) {
        hostPtr = host.c_str();
    }

    int res = getaddrinfo(hostPtr, service.c_str(), &hints, &result);
    if (res != 0) {
        throw TException() << "getaddrinfo failed: " << gai_strerror(res);
    }

    std::vector<TSocketAddress> addresses;

    for (struct addrinfo* rp = result; rp; rp = rp->ai_next) {
        addresses.emplace_back(rp->ai_addr, rp->ai_addrlen);
    }

    freeaddrinfo(result);

    if (addresses.empty()) {
        throw TException() << "getaddrinfo failed: no resolved addresses";
    }

    return addresses;
}
