#pragma once

#include <netdb.h>
#include <string>
#include <vector>
#include <type_traits>


class TSocketAddress {
public:
    TSocketAddress();
    TSocketAddress(const sockaddr* sa, size_t len);

    void InitPort();
    void InitHost();

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

    bool operator==(const TSocketAddress& other) const;
    bool operator!=(const TSocketAddress& other) const {
        return !(*this == other);
    };

private:
    sockaddr_storage Addr_;
    socklen_t Len_;
};

std::vector<TSocketAddress> GetAddrInfo(const std::string& host, const std::string& service, bool listener, const std::string& protocol);
TSocketAddress Resolve(const std::string& addressString);
