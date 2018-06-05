#include <sys/epoll.h>
#include <sys/signalfd.h>
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

void TEventLoop::EpollAddOrModify(TSocketHandle* handle, int events) {
    int op = EPOLL_CTL_MOD;
    if (!handle->Registered) {
        op = EPOLL_CTL_ADD;
        handle->Registered = true;

        if (Handles_.size() <= handle->Fd()) {
            Handles_.resize(handle->Fd() + 1);
        }

        Handles_[handle->Fd()] = handle->shared_from_this();
    }

    int res = EpollOp(op, handle->Fd(), events);

    if (res == -1) {
        Handles_[handle->Fd()] = nullptr;

        char error[4096];
        throw TException() << "cannot add socket to epoll: " << strerror_r(errno, error, sizeof(error));
    }

    handle->EpollEvents = events;
}

void TEventLoop::EpollModify(TSocketHandle* handle, int events) {
    int res = EpollOp(EPOLL_CTL_MOD, handle->Fd(), events);

    if (res == -1) {
        char error[4096];
        throw TException() << "EpollModify failed: " << strerror_r(errno, error, sizeof(error));
    }

    handle->EpollEvents = events;
}

TSocketHandlePtr TEventLoop::MakeTcp() {
    int fd = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

    if (fd < 0) {
        char error[4096];
        throw TException() << "tcp socket creating failed: " << strerror_r(errno, error, sizeof(error));
    }

    return std::make_shared<TSocketHandle>(this, fd);
}

void TEventLoop::Listen(TSocketHandle* handle, int backlog) {
    if (::listen(handle->Fd(), backlog) == -1) {
        char error[4096];
        throw TException() << "listen failed: " << strerror_r(errno, error, sizeof(error));
    }
    EpollAddOrModify(handle, handle->EpollEvents | EPOLLIN);
}

void TEventLoop::StartRead(TSocketHandle* handle) {
    EpollAddOrModify(handle, handle->EpollEvents | EPOLLIN);
}

void TEventLoop::StopRead(TSocketHandle* handle) {
    EpollAddOrModify(handle, handle->EpollEvents & ~EPOLLIN);
}

void TEventLoop::StartWrite(TSocketHandle* handle) {
    EpollAddOrModify(handle, handle->EpollEvents | EPOLLOUT);
}

void TEventLoop::Connect(TSocketHandle* handle) {
    if (::connect(handle->Fd(), handle->ConnectEndpoint.AddressAs<sockaddr>(), handle->ConnectEndpoint.Length()) == -1) {
        char error[4096];
        throw TException() << "connect failed: " << strerror_r(errno, error, sizeof(error));
    }
    EpollAddOrModify(handle, handle->EpollEvents | EPOLLOUT);
}

void TEventLoop::Close(int fd) {
    Handles_[fd] = nullptr;
}

void TEventLoop::DoAccept(TSocketHandle* handle) {
    sockaddr_storage addr;
    socklen_t len = sizeof(sockaddr_storage);

    int fd = ::accept4(handle->Fd(), reinterpret_cast<sockaddr*>(&addr), &len, SOCK_NONBLOCK);

    if (fd == -1 && fd != EAGAIN && fd != ECONNABORTED) {
        char error[4096];
        throw TException() << "error while accepting from fd=" << handle->Fd() << ": " << strerror_r(errno, error, sizeof(error));
    }

    TSocketHandlePtr accepted(new TSocketHandle(this, fd));
    accepted->SetAddress(TSocketAddress(reinterpret_cast<const sockaddr*>(&addr), len));
    handle->AcceptHandler(handle->shared_from_this(), std::move(accepted));
}

void TEventLoop::DoRead(TSocketHandle* handle) {
    if (handle->ReadDestination->Remaining() == 0) {
        EpollModify(handle, handle->EpollEvents & ~EPOLLIN);
        return;
    }

    ssize_t ret = ::read(handle->Fd(), handle->ReadDestination->End(), handle->ReadDestination->Remaining());

    if (ret < 0) {
        if (errno != EAGAIN) {
            // TODO
        }
    } else if (ret > 0) {
        handle->ReadDestination->Advance(ret);
    }

    handle->ReadHandler(handle->shared_from_this(), ret, ret == 0);
}

void TEventLoop::DoWrite(TSocketHandle* handle) {
    for (;;) {
        TMemoryRegion currentRegion = handle->WriteChain.CurrentMemoryRegion();

        if (currentRegion.Empty()) {
            EpollModify(handle, handle->EpollEvents & ~EPOLLOUT);
            handle->WriteHandler(handle->shared_from_this());
            break;
        }

        ssize_t res = write(handle->Fd(), currentRegion.Data(), currentRegion.Size());

        if (res == -1) {
            if (errno == EAGAIN) {
                break;
            } else {
                // TODO
            }
        }

        handle->WriteChain.Advance(res);
    }
}

void TEventLoop::DoConnect(TSocketHandle* handle) {
    handle->ConnectHandler(handle->shared_from_this());
    handle->ConnectHandler = nullptr;
    EpollAddOrModify(handle, handle->EpollEvents & ~EPOLLOUT);
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
                SignalHandlers_[sig](sig);
            }
        }
    }
}

void TEventLoop::Do() {
    static constexpr size_t MaxEvents = 1024;

    epoll_event events[MaxEvents];

    int res = ::epoll_wait(Fd_, events, MaxEvents, -1);
    if (res == -1 && errno != EINTR) {
        char error[4096];
        throw TException() << "epoll_wait failed: " << strerror_r(errno, error, sizeof(error));
    }

    for (int i = 0; i < res; i++) {
        int fd = events[i].data.fd;
        int fd_events = events[i].events;

        if (fd == SignalFd_) {
            DoSignal();
        } else {
            TSocketHandlePtr handle = Handles_[fd];

            if (fd_events & EPOLLIN) {
                if (handle->AcceptHandler) {
                    DoAccept(handle.get());
                } else if (handle->ReadHandler) {
                    DoRead(handle.get());
                }
            }

            if (fd_events & EPOLLOUT) {
                if (handle->ConnectHandler) {
                    DoConnect(handle.get());
                } else {
                    ASSERT(handle->WriteHandler);
                    DoWrite(handle.get());
                }
            }

            if (fd_events & EPOLLERR) {
            }
        }
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
