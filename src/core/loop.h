#pragma once

#include <sys/signalfd.h>

#include <functional>
#include <memory>
#include <queue>
#include <vector>
#include <atomic>
#include <csignal>
#include <list>

#include "handle.h"

class TSignalInfo {
public:
    TSignalInfo(const signalfd_siginfo& info)
        : Info_(info)
    {
    }

    int Signal() const {
        return Info_.ssi_signo;
    }

    pid_t Sender() const {
        return Info_.ssi_pid;
    }

private:
    signalfd_siginfo Info_;
};

using TSignalHandler = std::function<void(TSignalInfo info)>;
using TCleanupHandler = std::function<void()>;

using TCleanupHandle = std::list<TCleanupHandler>::iterator;

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

    TCleanupHandle Cleanup(TCleanupHandler handler) {
        CleanupHandlers_.push_back(std::move(handler));
        return std::next(CleanupHandlers_.end(), -1);
    }

    void RunForever();
    void Shutdown();

    void Cancel(TCleanupHandle handle);

private:
    int EpollOp(int op, int fd, int events);
    void EpollAdd(TSocketHandlePtr handle, int events);
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
    std::list<TCleanupHandler> CleanupHandlers_;
};
