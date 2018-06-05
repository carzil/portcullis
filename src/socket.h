#pragma once

#include <string>
#include <memory>
#include <string.h>
#include <netinet/ip.h>
#include <sys/socket.h>
#include <unistd.h>
#include <vector>
#include <boson/boson.h>
#include <boson/select.h>

#include "exception.h"
#include "utils.h"


/**
 * Represents a non-blocking socket.
 */
class TSocket : public TMoveOnly {
public:
    static std::vector<TSocketAddress> GetAddrInfo(const std::string& host, const std::string& service, bool listener = false, const std::string& protocol = "tcp");

    TSocket() = default;

    ~TSocket() {
        if (Fd_ != -1) {
            close(Fd_);
        }
    }

    TSocket(TSocket&& sock) noexcept {
        Fd_ = sock.Fd_;
        Addr_ = std::move(sock.Addr_);
        Data_ = sock.Data_;
        sock.Fd_ = -1;
    }

    TSocket& operator=(TSocket&& sock) noexcept {
        Fd_ = sock.Fd_;
        Addr_ = std::move(sock.Addr_);
        Data_ = sock.Data_;
        sock.Fd_ = -1;
        return *this;
    }

    static TSocket TcpSocket() {
        TSocket sock;
        sock.Fd_ = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (sock.Fd_ < 0) {
            char error[4096];
            throw TException() << "socket failed" << strerror_r(errno, error, sizeof(error));
        }
        return sock;
    }

    size_t Read(TSocketBuffer& buffer);

    template<class Consumer>
    auto EvRead(TSocketBuffer& buffer, Consumer&& consumer) {
        return boson::event_read(Fd(), buffer.End(), buffer.Remaining(), [consumer{std::move(consumer)}](ssize_t res) {
            consumer(res);
        });
    }

    size_t Write(const void* buf, size_t maxSize, size_t& actuallyWritten);

    void WriteFull(const TSocketBuffer& buffer) {
        size_t written = 0;
        while (written < buffer.Size()) {
            size_t done;
            Write(buffer.Data() + written, buffer.Size() - written, done);
            std::cerr << written << " " << done << buffer.Size() << std::endl;
            written += done;
        }
    }

    bool Accept(TSocket* result) const;

    void Bind(const TSocketAddress& addr);
    void Listen(int backlog = SOMAXCONN);
    void Connect(const TSocketAddress& addr);

    void ReuseAddr();
    void Blocking(bool blocking);

    void Close();

    inline const TSocketAddress& Address() const {
        return Addr_;
    }

    int Fd() const {
        return Fd_;
    }

    template<typename T>
    const T DataAs() const {
        return reinterpret_cast<const T>(Data_);
    }

    void SetData(void* data) {
        Data_ = data;
    }

private:
    int Fd_ = -1;
    TSocketAddress Addr_;
    void* Data_ = nullptr;
};
