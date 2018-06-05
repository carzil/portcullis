#pragma once

#include <functional>
#include <memory>
#include <queue>
#include <vector>
#include <atomic>
#include <csignal>

#include "handle.h"

using TSignalHandler = std::function<void(int signal)>;

class TEventLoop {
public:
    TEventLoop();
    ~TEventLoop();

    std::shared_ptr<TSocketHandle> MakeTcp();

    void Listen(std::shared_ptr<TSocketHandle> handle, int backlog);
    void StartRead(std::shared_ptr<TSocketHandle> handle);
    void StopRead(std::shared_ptr<TSocketHandle> handle);
    void Close(TSocketHandle* handle);

    void Signal(int sig, TSignalHandler handler);

    void RunForever();
    void Shutdown();

private:
    int EpollOp(int op, int fd, int events);
    void EpollAddOrModify(std::shared_ptr<TSocketHandle> handle, int events);
    void EpollModify(TSocketHandle* handle, int events);
    void Do();
    std::shared_ptr<TSocketHandle> DoAccept(TSocketHandle* handle);
    void DoRead(TSocketHandle* handle);
    void DoSignal();

    int Fd_ = -1;
    std::vector<std::shared_ptr<TSocketHandle>> Handles_;
    std::atomic<bool> Running_;

    int SignalFd_ = -1;
    sigset_t SignalsHandled_;
    std::vector<TSignalHandler> SignalHandlers_;
};
