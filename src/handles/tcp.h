#pragma once

#include "common.h"

class TTcpHandle;
using TTcpHandlePtr = std::shared_ptr<TTcpHandle>;

class TTcpHandle : public THandle {
public:
    using THandle::THandle;

    static TTcpHandlePtr Create(bool ipv6);

    void Bind(const TSocketAddress& addr);
    void Listen(int backlog);

    TResult<TTcpHandlePtr> Accept(TReactor::TDeadline deadline = TReactor::TDeadline::max());
    TResult<bool> Connect(const TSocketAddress& endpoint, TReactor::TDeadline deadline = TReactor::TDeadline::max());

    void ReuseAddr();
    void ReusePort();

    void Ipv6Only();

    void ShutdownAll();
    void ShutdownRead();
    void ShutdownWrite();

    const TSocketAddress& PeerAddress() const {
        return PeerAddress_;
    }

    const TSocketAddress& SockAddress() const {
        return SockAdderess_;
    }

private:
    void EnableSockOpt(int level, int optname);

    TSocketAddress SockAdderess_;
    TSocketAddress PeerAddress_;
};

class TTcpListener {
public:
    TTcpListener(std::shared_ptr<spdlog::logger> logger)
        : Logger_(std::move(logger))
    {}

    void ReuseAddr() {
        ReuseAddr_ = true;
    }

    void Bind(std::vector<TSocketAddress> addresses) {
        for (const TSocketAddress& addr : addresses) {
            TTcpHandlePtr handle = TTcpHandle::Create(addr.Ipv6());
            if (ReuseAddr_) {
                handle->ReuseAddr();
            }
            if (addr.Ipv6()) {
                /* by default IPv6 socket listens on both IPv6 and IPv4 addresses */
                handle->Ipv6Only();
            }
            handle->Bind(addr);
            Listeners_.emplace_back(std::move(handle));
        }
    }

    void OnAccepted(std::function<void(TTcpHandlePtr)> onAccepted) {
        OnAccepted_ = std::move(onAccepted);
    }

    void Run(size_t backlog) {
        std::vector<TCoroutine*> coroutines;
        for (TTcpHandlePtr listener : Listeners_) {
            coroutines.push_back(Reactor()->StartAwaitableCoroutine(
                std::bind(&TTcpListener::RunListener, this, std::move(listener), backlog))
            );
        }

        Reactor()->AwaitAll(coroutines);
    }

private:
    void RunListener(TTcpHandlePtr listener, size_t backlog) {
        listener->Listen(backlog);

        Logger_->info("listening on {}:{}", listener->SockAddress().Host(), listener->SockAddress().Port());

        while (true) {
            TResult<TTcpHandlePtr> result = listener->Accept();

            if (!result) {
                if (result.Canceled()) {
                    return;
                } else {
                    Logger_->critical("cannot accept new client: {}", ErrorDescription(result.Error()));
                }
            }

            OnAccepted_(std::move(result.Result()));
        }
    }

    std::function<void(TTcpHandlePtr)> OnAccepted_;
    std::vector<TTcpHandlePtr> Listeners_;
    std::shared_ptr<spdlog::logger> Logger_;
    bool ReuseAddr_ = false;
};
