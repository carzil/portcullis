#pragma once

#include <netdb.h>
#include <string>
#include <vector>
#include <type_traits>


class TSocketAddress {
public:
    TSocketAddress();
    TSocketAddress(const sockaddr* sa, size_t len);

    uint16_t Port() const;
    std::string Host() const;

    template<class T>
    T AddressAs() {
        return reinterpret_cast<T>(&Addr_);
    }

    template<class T>
    T AddressAs() const {
        return reinterpret_cast<T>(&Addr_);
    }

    socklen_t Length() const {
        return Len_;
    }

    socklen_t& Length() {
        return Len_;
    }

    bool Ipv6() const {
        return Addr_.ss_family == AF_INET6;
    }

    bool Ipv4() const {
        return Addr_.ss_family == AF_INET;
    }

    bool operator==(const TSocketAddress& other) const;
    bool operator!=(const TSocketAddress& other) const {
        return !(*this == other);
    };

private:
    sockaddr_storage Addr_;
    socklen_t Len_;
};

enum EIpVersionMode {
    V4_AND_V6 = 0,
    V4_ONLY = 1,
    V6_ONLY = 2,
};

std::vector<TSocketAddress> GetAddrInfo(const std::string& host, const std::string& service, bool listener, const std::string& protocol, EIpVersionMode = V4_AND_V6);

TSocketAddress Resolve(const std::string& addressString, EIpVersionMode mode);

TSocketAddress ResolveV46(const std::string& addressString);
TSocketAddress ResolveV4(const std::string& addressString);
TSocketAddress ResolveV6(const std::string& addressString);
