#include <vector>
#include <cstring>

#include <arpa/inet.h>
#include <netinet/ip.h>
#include <util/network/address.h>
#include <util/exception.h>

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
            throw TException() << "unknown socket family: " << sa->sa_family;
    };
}

bool TSocketAddress::operator==(const TSocketAddress& other) const {
    const sockaddr* sa1 = AddressAs<const sockaddr*>();
    const sockaddr* sa2 = other.AddressAs<const sockaddr*>();

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
            throw TException() << "unknown socket family: " << sa->sa_family;
    };
    if (host) {
        return std::string(host);
    } else {
        return std::string();
    }
}

std::vector<TSocketAddress> GetAddrInfo(const std::string& host, const std::string& service, bool listener, const std::string& protocol, EIpVersionMode ipVersion) {
    int ai_flags = listener ? (AI_PASSIVE | AI_ADDRCONFIG) : 0;
    int ai_protocol = 0;
    int ai_family = 0;
    int ai_socktype = 0;

    switch (ipVersion) {
        case V4_AND_V6:
            ai_family = AF_UNSPEC;
            break;
        case V6_ONLY:
            ai_family = AF_INET6;
            break;
        case V4_ONLY:
            ai_family = AF_INET;
            break;
        default:
            ASSERT(false);
    }

    if (protocol == "tcp") {
        ai_socktype = SOCK_STREAM;
    } else if (protocol == "udp") {
        ai_socktype = SOCK_DGRAM;
    } else {
        throw TException() << "unknown protocol '" << protocol << "'";
    }

    struct addrinfo* result;
    struct addrinfo hints;

    hints.ai_flags = ai_flags;
    hints.ai_family = ai_family;
    hints.ai_protocol = ai_protocol;
    hints.ai_socktype = ai_socktype;

    const char* hostPtr = nullptr;
    if (host.size() > 0) {
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

TSocketAddress Resolve(const std::string& addressString, EIpVersionMode mode) {
    size_t protoDelimPos = addressString.find("://");
    if (protoDelimPos == std::string::npos) {
        throw TException() << "malformed resolve-string: '" << addressString << "'";
    }

    std::string proto = addressString.substr(0, protoDelimPos);

    size_t hostDelimPos = addressString.find(":", protoDelimPos + 3);
    if (hostDelimPos == std::string::npos) {
        throw TException() << "malformed resolve-string: '" << addressString << "'";
    }

    std::string host = addressString.substr(protoDelimPos + 3, hostDelimPos - (protoDelimPos + 3));
    std::string service = addressString.substr(hostDelimPos + 1);

    TSocketAddress addr = GetAddrInfo(host, service, false, proto, mode)[0];

    return addr;
}

TSocketAddress ResolveV46(const std::string& addressString) {
    return Resolve(addressString, V4_AND_V6);
}

TSocketAddress ResolveV4(const std::string& addressString) {
    return Resolve(addressString, V4_ONLY);
}

TSocketAddress ResolveV6(const std::string& addressString) {
    return Resolve(addressString, V6_ONLY);
}
