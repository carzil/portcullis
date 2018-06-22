#include <sys/epoll.h>
#include <unistd.h>

#include <memory>

#include "loop.h"
#include "exception.h"
#include "utils.h"


TEventLoop::TEventLoop() {
    Fd_ = epoll_create1(0);

    if (Fd_ == -1) {
        char error[4096];
        throw TException() << "cannot create epoll instance: " << strerror_r(errno, error, sizeof(error));
    }

    ::sigemptyset(&SignalsHandled_);
}

TEventLoop::~TEventLoop() {
    ::close(Fd_);
    ::close(SignalFd_);
}

int TEventLoop::EpollOp(int op, int fd, int events) {
    epoll_event ctl_event;
    ctl_event.events = events;
    ctl_event.data.fd = fd;
    return ::epoll_ctl(Fd_, op, fd, &ctl_event);
}

void TEventLoop::EpollAdd(TSocketHandlePtr handle, int events) {
    if (Handles_.size() <= handle->Fd()) {
        Handles_.resize(handle->Fd() + 1);
    }

    int res = EpollOp(EPOLL_CTL_ADD, handle->Fd(), events);

    if (res == -1) {
        char error[4096];
        throw TException() << "cannot add socket to epoll: " << strerror_r(errno, error, sizeof(error));
    }

    Handles_[handle->Fd()] = std::move(handle);
}

TSocketHandlePtr TEventLoop::MakeTcp() {
    int fd = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

    if (fd < 0) {
        char error[4096];
        throw TException() << "tcp socket creating failed: " << strerror_r(errno, error, sizeof(error));
    }

    SetNonBlocking(fd);

    TSocketHandlePtr handle(new TSocketHandle(this, fd));

    EpollAdd(handle, EPOLLIN | EPOLLOUT | EPOLLET);

    return handle;
}

void TEventLoop::Listen(TSocketHandle* handle, int backlog) {
    if (::listen(handle->Fd(), backlog) == -1) {
        char error[4096];
        throw TException() << "listen failed: " << strerror_r(errno, error, sizeof(error));
    }
}

void TEventLoop::StartRead(TSocketHandle* handle) {
    if (!handle->Active()) {
        return;
    }
    handle->EnabledEvents |= TSocketHandle::ReadEvent;
    if (handle->Ready(TSocketHandle::ReadEvent)) {
        DoRead(handle);
    }
}

void TEventLoop::StopRead(TSocketHandle* handle) {
    handle->EnabledEvents &= ~TSocketHandle::ReadEvent;
}

void TEventLoop::StartWrite(TSocketHandle* handle) {
    if (!handle->Active()) {
        return;
    }
    handle->EnabledEvents |= TSocketHandle::WriteEvent;
    if (handle->Ready(TSocketHandle::WriteEvent)) {
        DoWrite(handle);
    }
}

void TEventLoop::Connect(TSocketHandle* handle) {
    int ret = ::connect(handle->Fd(), handle->ConnectEndpoint.AddressAs<sockaddr>(), handle->ConnectEndpoint.Length());
    if (ret == -1 && errno != EINPROGRESS) {
        char error[4096];
        throw TException() << "connect failed: " << strerror_r(errno, error, sizeof(error));
    } else if (ret == 0) {
        DoConnect(handle);
    }
}

void TEventLoop::Close(int fd) {
    Handles_[fd] = nullptr;
}

void TEventLoop::Cancel(TCleanupHandle handle) {
    CleanupHandlers_.erase(handle);
}

void TEventLoop::DoAccept(TSocketHandle* handle) {
    for (;;) {
        sockaddr_storage addr;
        socklen_t len = sizeof(sockaddr_storage);
        int fd = ::accept4(handle->Fd(), reinterpret_cast<sockaddr*>(&addr), &len, SOCK_NONBLOCK);

        if (fd == -1) {
            if (errno == EAGAIN) {
                break;
            }
            char error[4096];
            throw TException() << "error while accepting from fd=" << handle->Fd() << ": " << strerror_r(errno, error, sizeof(error));
        }

        TSocketHandlePtr accepted(new TSocketHandle(this, fd));
        accepted->SetAddress(TSocketAddress(reinterpret_cast<const sockaddr*>(&addr), len));
        EpollAdd(accepted, EPOLLIN | EPOLLOUT | EPOLLET);
        handle->AcceptHandler(handle->shared_from_this(), std::move(accepted));
    }
}

void TEventLoop::DoRead(TSocketHandle* handle) {
    ASSERT(handle->Active());
    if (handle->ReadDestination->Remaining() == 0) {
        handle->EnabledEvents &= ~TSocketHandle::ReadEvent;
        return;
    }

    ssize_t ret = ::read(handle->Fd(), handle->ReadDestination->End(), handle->ReadDestination->Remaining());

    if (ret < 0) {
        if (errno == EAGAIN) {
            handle->ReadyEvents &= ~TSocketHandle::ReadEvent;
        } else {
            return;
        }
    } else if (ret > 0) {
        handle->ReadDestination->Advance(ret);
    }

    handle->ReadHandler(handle->shared_from_this(), ret, ret == 0);
}

void TEventLoop::DoWrite(TSocketHandle* handle) {
    ASSERT(handle->Active());
    for (;;) {
        TMemoryRegion currentRegion = handle->WriteChain.CurrentMemoryRegion();

        if (currentRegion.Empty()) {
            handle->WriteHandler(handle->shared_from_this());
            break;
        }

        ssize_t res = write(handle->Fd(), currentRegion.Data(), currentRegion.Size());

        if (res == -1) {
            if (errno == EAGAIN) {
                handle->ReadyEvents &= ~TSocketHandle::WriteEvent;
                break;
            } else {
                return;
            }
        }

        handle->WriteChain.Advance(res);
    }
}

void TEventLoop::DoConnect(TSocketHandle* handle) {
    ASSERT(handle->Active());
    TSocketHandlePtr ptr = handle->shared_from_this();
    handle->ConnectHandler(std::move(ptr));
    handle->ConnectHandler = nullptr;
}

void TEventLoop::DoSignal() {
    for (;;) {
        signalfd_siginfo sigs[16];

        int ret = ::read(SignalFd_, &sigs, sizeof(sigs));

        if (ret < 0) {
            break;
        }

        int total_signals = ret / sizeof(signalfd_siginfo);

        for (int i = 0; i < total_signals; i++) {
            int sig = sigs[i].ssi_signo;
            if (SignalHandlers_[sig]) {
                SignalHandlers_[sig](TSignalInfo(sigs[i]));
            }
        }
    }
}

void TEventLoop::Do() {
    static constexpr size_t MaxEvents = 1024;

    epoll_event events[MaxEvents];

    int res = ::epoll_wait(Fd_, events, MaxEvents, -1);
    if (res == -1) {
        if (errno == EINTR) {
            return;
        }
        char error[4096];
        throw TException() << "epoll_wait failed: " << strerror_r(errno, error, sizeof(error));
    }

    for (int i = 0; i < res; i++) {
        int fd = events[i].data.fd;

        if (fd == SignalFd_) {
            continue;
        }

        TSocketHandle* handle = Handles_[fd].get();

        if (!handle) {
            continue;
        }

        if (events[i].events & EPOLLIN) {
            handle->ReadyEvents |= TSocketHandle::ReadEvent;
        }

        if (events[i].events & EPOLLOUT) {
            handle->ReadyEvents |= TSocketHandle::WriteEvent;
        }
    }

    for (int i = 0; i < res; i++) {
        int fd = events[i].data.fd;
        int fd_events = events[i].events;

        if (fd == SignalFd_) {
            DoSignal();
        } else {
            TSocketHandlePtr handle = Handles_[fd];

            if (!handle || !handle->Active()) {
                continue;
            }

            if (fd_events & EPOLLIN) {
                if (handle->AcceptHandler) {
                    DoAccept(handle.get());
                } else if (handle->Enabled(TSocketHandle::ReadEvent) && handle->ReadHandler) {
                    DoRead(handle.get());
                }
            }

            if (!handle->Active()) {
                continue;
            }

            if (fd_events & EPOLLOUT) {
                if (handle->ConnectHandler) {
                    DoConnect(handle.get());
                } else if (handle->Enabled(TSocketHandle::WriteEvent) && handle->WriteHandler) {
                    DoWrite(handle.get());
                }
            }
        }
    }

    for (const TCleanupHandler& cleanupHandler : CleanupHandlers_) {
        cleanupHandler();
    }
}

void TEventLoop::Signal(int sig, TSignalHandler handler) {
    ::sigaddset(&SignalsHandled_, sig);
    ::sigprocmask(SIG_BLOCK, &SignalsHandled_, NULL);

    if (SignalFd_ != -1) {
        ::close(SignalFd_);
    }

    SignalFd_ = ::signalfd(-1, &SignalsHandled_, SFD_NONBLOCK);

    if (SignalFd_ == -1) {
        char error[4096];
        throw TException() << "signalfd failed: " << strerror_r(errno, error, sizeof(error));
    }

    EpollOp(EPOLL_CTL_ADD, SignalFd_, EPOLLIN);

    if (SignalHandlers_.empty()) {
        SignalHandlers_.resize(SIGRTMAX);
    }

    ASSERT(!SignalHandlers_[sig]);
    SignalHandlers_[sig] = handler;
}

void TEventLoop::RunForever() {
    Running_.store(true);
    while (Running_.load()) {
        Do();
    }
}

void TEventLoop::Shutdown() {
    Running_.store(false);
}
