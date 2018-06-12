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
}

uint16_t TSocketAddress::Port() const {
    const sockaddr* sa = reinterpret_cast<const sockaddr*>(&Addr_);
    switch (sa->sa_family) {
        case AF_INET:
            return ntohs(reinterpret_cast<const sockaddr_in*>(sa)->sin_port);
        case AF_INET6:
            return ntohs(reinterpret_cast<const sockaddr_in6*>(sa)->sin6_port);
        default:
            throw TException() << "unknown socket familly: " << sa->sa_family;
    };
}

bool TSocketAddress::operator==(const TSocketAddress& other) const {
    const sockaddr* sa1 = AddressAs<const sockaddr>();
    const sockaddr* sa2 = other.AddressAs<const sockaddr>();

    if (sa1->sa_family != sa2->sa_family) {
        return false;
    }

    if (sa1->sa_family == AF_INET) {
        const sockaddr_in* in1 = reinterpret_cast<const sockaddr_in*>(sa1);
        const sockaddr_in* in2 = reinterpret_cast<const sockaddr_in*>(sa2);
        return in1->sin_port == in2->sin_port && memcmp(&in1->sin_addr, &in2->sin_addr, sizeof(struct in_addr)) == 0;
    } else if (sa2->sa_family == AF_INET6) {
        const sockaddr_in6* in1 = reinterpret_cast<const sockaddr_in6*>(sa1);
        const sockaddr_in6* in2 = reinterpret_cast<const sockaddr_in6*>(sa2);
        return in1->sin6_port == in2->sin6_port && memcmp(&in1->sin6_addr, &in2->sin6_addr, sizeof(struct in6_addr)) == 0;
    }

    throw TException() << "unknown family " << sa1->sa_family;
}

std::string TSocketAddress::Host() const {
    char hostStr[std::max(INET_ADDRSTRLEN, INET6_ADDRSTRLEN)];
    const sockaddr* sa = reinterpret_cast<const sockaddr*>(&Addr_);
    const sockaddr_in* in = reinterpret_cast<const sockaddr_in*>(sa);
    const sockaddr_in6* in6 = reinterpret_cast<const sockaddr_in6*>(sa);
    const char* host = nullptr;
    switch (sa->sa_family) {
        case AF_INET:
            host = ::inet_ntop(sa->sa_family, &in->sin_addr, reinterpret_cast<char*>(&hostStr), sizeof(hostStr));
            break;
        case AF_INET6:
            host = ::inet_ntop(sa->sa_family, &in6->sin6_addr, reinterpret_cast<char*>(&hostStr), sizeof(hostStr));
            break;
        default:
            throw TException() << "unknown socket familly: " << sa->sa_family;
    };
    if (host != nullptr) {
        return std::string(host);
    } else {
        return std::string();
    }
}


TSocketHandle::~TSocketHandle() {
    Close();
}

void TSocketHandle::Listen(TAcceptHandler handler, int backlog) {
    AcceptHandler = std::move(handler);
    Loop_->Listen(this, backlog);
}

void TSocketHandle::Read(TSocketBuffer* readDest, TReadHandler handler) {
    ReadDestination = readDest;
    ReadHandler = std::move(handler);
    Loop_->StartRead(this);
}

void TSocketHandle::PauseRead() {
    Loop_->StopRead(this);
}

void TSocketHandle::RestartRead() {
    Loop_->StartRead(this);
}

void TSocketHandle::Write(TMemoryRegionChain chain, TWriteHandler handler) {
    WriteChain = std::move(chain);
    WriteHandler = std::move(handler);
    Loop_->StartWrite(this);
}

void TSocketHandle::Connect(TSocketAddress endpoint, TConnectHandler handler) {
    ConnectHandler = std::move(handler);
    ConnectEndpoint = std::move(endpoint);
    Loop_->Connect(this);
}

void TSocketHandle::StopRead() {
    ReadHandler = nullptr;
    Loop_->StopRead(this);
}

void TSocketHandle::Close() {
    ReadHandler = nullptr;
    WriteHandler = nullptr;
    AcceptHandler = nullptr;
    ConnectHandler = nullptr;
    ErrorHandler = nullptr;

    ReadyEvents = 0;
    EnabledEvents = 0;

    if (Fd_ != -1) {
        int fd = Fd_;
        ::close(fd);
        Fd_ = -1;
        Loop_->Close(fd);
    }
}

void TSocketHandle::Bind(const TSocketAddress& addr) {
    int res = ::bind(Fd_, addr.AddressAs<const sockaddr>(), addr.Length());
    if (res < 0) {
        char error[4096];
        throw TException() << "bind failed: " << strerror_r(errno, error, sizeof(error));
    }
    SetAddress(addr);
}

void TSocketHandle::ReuseAddr() {
    int enable = 1;
    if (setsockopt(Fd(), SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int)) < 0) {
        char error[4096];
        throw TException() << "setsockopt failed: " << strerror_r(errno, error, sizeof(error));
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
