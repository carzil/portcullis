#include "utils.h"

bool AddrsEqual(const sockaddr* sa1, const sockaddr* sa2) {
    if (sa1->sa_family != sa2->sa_family) {
        return false;
    }

    if (sa1->sa_family == AF_INET) {
        const sockaddr_in* in1 = reinterpret_cast<const sockaddr_in*>(sa1);
        const sockaddr_in* in2 = reinterpret_cast<const sockaddr_in*>(sa2);
        return in1->sin_port == in2->sin_port && in1->sin_addr == in2->sin_addr;
    } else if (sa2->sa_family == AF_INET6) {
        const sockaddr_in* in1 = reinterpret_cast<const sockaddr_in6*>(sa1);
        const sockaddr_in* in2 = reinterpret_cast<const sockaddr_in6*>(sa2);
        return in1->sin6_port == in2->sin6_port && in1->sin6_addr == in2->sin6_addr;
    }
}
