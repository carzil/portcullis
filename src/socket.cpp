#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <fcntl.h>

#include "socket.h"


size_t TSocket::Read(TSocketBuffer& buffer) {
    ssize_t res = boson::read(Fd(), buffer.End(), buffer.Remaining());

    if (res == -1) {
        char error[4096];
        throw TException() << "read failed: " << strerror_r(errno, error, sizeof(error));
    }

    buffer.Increase(res);

    return res;
}

size_t TSocket::Write(const void* buf, size_t maxSize, size_t& actuallyWritten) {
    ssize_t res = boson::write(Fd(), buf, maxSize);

    if (res != 0) {
        if (errno == EAGAIN) {
            return false;
        }
        char error[4096];
        throw TException() << "write failed: " << strerror_r(errno, error, sizeof(error));
    }

    return res;
}

void TSocket::Connect(const TSocketAddress& endpoint) {
    size_t res = boson::connect(Fd(), endpoint.AddressAs<const sockaddr*>(), endpoint.Length());
    if (res != 0) {
        char error[4096];
        throw TException() << "connect failed: " << strerror_r(errno, error, sizeof(error));
    }
}

void TSocket::ReuseAddr() {
    int enable = 1;
    if (setsockopt(Fd(), SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int)) < 0) {
        char error[4096];
        throw TException() << "setsockopt failed: " << strerror_r(errno, error, sizeof(error));
    }
}

void TSocket::Blocking(bool blocking) {
    if (!blocking) {
        SetNonBlocking(Fd());
    }
}

void TSocket::Bind(const TSocketAddress& addr) {
    int res = bind(Fd(), addr.AddressAs<const sockaddr*>(), addr.Length());
    if (res < 0) {
        char error[4096];
        throw TException() << "bind failed: " << strerror_r(errno, error, sizeof(error));
    }
}

void TSocket::Listen(int backlog) {
    if (listen(Fd(), backlog) == -1) {
        char error[4096];
        throw TException() << "listen failed: " << strerror_r(errno, error, sizeof(error));;
    }
}

bool TSocket::Accept(TSocket* socket) const {
    struct sockaddr_storage addr;
    socklen_t in_addr_len = sizeof(addr);

    int new_fd = boson::accept(Fd(), reinterpret_cast<struct sockaddr*>(&addr), &in_addr_len);

    if (new_fd == -1) {
        if (errno == EAGAIN) {
            return false;
        }
        char error[4096];
        throw TException() << "accept failed: " << strerror_r(errno, error, sizeof(error));;
    }

    socket->Fd_ = new_fd;
    socket->Addr_ = TSocketAddress(reinterpret_cast<struct sockaddr*>(&addr), in_addr_len);

    return true;
};

void TSocket::Close() {
    close(Fd_);
    Fd_ = -1;
}
