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

void TEventLoop::EpollAddOrModify(std::shared_ptr<TSocketHandle> handle, int events) {
    int op = EPOLL_CTL_MOD;
    if (!handle->Registered) {
        op = EPOLL_CTL_ADD;
        handle->Registered = true;

        if (Handles_.size() <= handle->Fd()) {
            Handles_.resize(handle->Fd() + 1);
        }

        Handles_[handle->Fd()] = handle;
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

std::shared_ptr<TSocketHandle> TEventLoop::MakeTcp() {
    int fd = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

    if (fd < 0) {
        char error[4096];
        throw TException() << "tcp socket creating failed: " << strerror_r(errno, error, sizeof(error));
    }

    return std::make_shared<TSocketHandle>(this, fd);
}

void TEventLoop::Listen(std::shared_ptr<TSocketHandle> handle, int backlog) {
    if (::listen(handle->Fd(), backlog) == -1) {
        char error[4096];
        throw TException() << "listen failed: " << strerror_r(errno, error, sizeof(error));;
    }
    EpollAddOrModify(std::move(handle), handle->EpollEvents | EPOLLIN);
}

void TEventLoop::StartRead(std::shared_ptr<TSocketHandle> handle) {
    EpollAddOrModify(std::move(handle), handle->EpollEvents | EPOLLIN);
}

void TEventLoop::StartWrite(std::shared_ptr<TSocketHandle> handle) {
    EpollAddOrModify(std::move(handle), handle->EpollEvents | EPOLLOUT);
}

void TEventLoop::Close(TSocketHandle* handle) {
    Handles_[handle->Fd()] = nullptr;
}

std::shared_ptr<TSocketHandle> TEventLoop::DoAccept(TSocketHandle* handle) {
    sockaddr_storage addr;
    socklen_t len = sizeof(sockaddr_storage);

    int fd = ::accept4(handle->Fd(), reinterpret_cast<sockaddr*>(&addr), &len, SOCK_NONBLOCK);

    if (fd == -1 && fd != EAGAIN && fd != ECONNABORTED) {
        char error[4096];
        throw TException() << "error while accepting from fd=" << handle->Fd() << ": " << strerror_r(errno, error, sizeof(error));
    }

    std::shared_ptr<TSocketHandle> accepted(new TSocketHandle(this, fd));
    accepted->SetAddress(TSocketAddress(reinterpret_cast<const sockaddr*>(&addr), len));
    return accepted;
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

    bool eof = ret == 0;
    handle->ReadHandler(handle->shared_from_this(), ret, eof);
    if (eof) {
        EpollModify(handle, handle->EpollEvents & ~EPOLLIN);
    }
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
            std::shared_ptr<TSocketHandle> handle = Handles_[fd];

            if (fd_events & EPOLLIN) {
                if (handle->AcceptHandler) {
                    std::shared_ptr<TSocketHandle> connected = DoAccept(handle.get());
                    handle->AcceptHandler(std::move(handle), std::move(connected));
                } else if (handle->ReadHandler) {
                    DoRead(handle.get());
                }
            }

            if (fd_events & EPOLLOUT) {
                ASSERT(handle->WriteHandler);
                DoWrite(handle.get());
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
