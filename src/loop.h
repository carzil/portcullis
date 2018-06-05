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

    TSocketHandlePtr MakeTcp();

    void Listen(TSocketHandle* handle, int backlog);

    void StartRead(TSocketHandle* handle);
    void StopRead(TSocketHandle* handle);

    void StartWrite(TSocketHandle* handle);
    void Connect(TSocketHandle* handle);
    void Close(int fd);

    void Signal(int sig, TSignalHandler handler);

    void RunForever();
    void Shutdown();

private:
    int EpollOp(int op, int fd, int events);
    void EpollAddOrModify(TSocketHandle* handle, int events);
    void EpollModify(TSocketHandle* handle, int events);
    void Do();
    void DoAccept(TSocketHandle* handle);
    void DoRead(TSocketHandle* handle);
    void DoWrite(TSocketHandle* handle);
    void DoConnect(TSocketHandle* handle);
    void DoSignal();

    int Fd_ = -1;
    std::vector<TSocketHandlePtr> Handles_;
    std::atomic<bool> Running_;

    int SignalFd_ = -1;
    sigset_t SignalsHandled_;
    std::vector<TSignalHandler> SignalHandlers_;
};
