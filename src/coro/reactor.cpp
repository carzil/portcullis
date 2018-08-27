#include "reactor.h"

#include <sys/epoll.h>
#include <sys/signalfd.h>

static TReactor* CurrentReactor = nullptr;
static TCoroutine* CurrentCoro = nullptr;

TReactor* Reactor() {
    ASSERT(CurrentReactor);
    return CurrentReactor;
}

/*
 * Wrapper for launching coroutine.
 */
void TReactor::CoroWrapper() {
    TCoroutine* coro = CurrentCoro;
    coro->Context.OnStart();
    /* unhandled exception here leads to std::terminate */
    coro->Entry();
    coro->Reactor->Finish(coro);
}

TReactor::TReactor(std::shared_ptr<spdlog::logger> logger)
    : Logger_(std::move(logger))
{
    EpollFd_ = epoll_create1(EPOLL_CLOEXEC);
    if (EpollFd_ == -1) {
        ThrowErrno("epoll_create failed");
    }

    std::fill(SignalHandlers_.begin(), SignalHandlers_.end(), nullptr);

    InitialCoro_.Reactor = this;
    InitialCoro_.Context.MarkAsMain();
}

TCoroutine* TReactor::Current() const {
    return CurrentCoro;
}

TCoroutine* TReactor::StartCoroutine(TCoroEntry entry) {
    std::unique_ptr<TCoroutine> coro = std::make_unique<TCoroutine>(this, std::move(entry));
    TCoroutine* coroPtr = coro.get();
    ScheduledNextCoroutines_.push_back(coroPtr);
    ActiveCoroutines_.push_back(std::move(coro));
    coroPtr->ListIter = std::next(ActiveCoroutines_.end(), -1);
    SPDLOG_DEBUG(Logger_, "started coroutine {}", reinterpret_cast<void*>(coroPtr));
    return coroPtr;
}

void TReactor::Finish(TCoroutine* coro) {
    TCoroutine* awaiter = coro->Awaiter;
    TSavedContext oldContext = CurrentCoro->Context;

    FinishedCoroutines_.push_back(std::move(*coro->ListIter));
    ActiveCoroutines_.erase(coro->ListIter);

    SPDLOG_DEBUG(Logger_, "{} finished", reinterpret_cast<void*>(coro));

    if (ActiveCoroutines_.empty()) {
        SPDLOG_DEBUG(Logger_, "no active coroutines left");
        oldContext.SwitchTo(InitialCoro_.Context, nullptr, 0, true);
    }

    if (awaiter) {
        awaiter->Wakeup();
    }

    SwitchCoroutine(true);
}

void TReactor::Yield() {
    SPDLOG_DEBUG(Logger_, "{} yielded", reinterpret_cast<void*>(CurrentCoro));
    ScheduledNextCoroutines_.push_back(CurrentCoro);
    SwitchCoroutine(false);
}

void TReactor::Run() {
    if (ActiveCoroutines_.empty()) {
        return;
    }
    CurrentCoro = &InitialCoro_;
    CurrentReactor = this;
    SwitchCoroutine(false);
}

void TReactor::SwitchCoroutine(bool exitOld) {
    CurrentCoro->Awake = false;

    if (ScheduledCoroutines_.empty()) {
        if (!ScheduledNextCoroutines_.empty()) {
            ScheduledCoroutines_ = std::move(ScheduledNextCoroutines_);
            DoPoll();
        } else {
            while (ScheduledNextCoroutines_.empty()) {
                DoPoll();
            }
            ScheduledCoroutines_ = std::move(ScheduledNextCoroutines_);
        }
    }

    TCoroutine* ready = ScheduledCoroutines_.front();
    ScheduledCoroutines_.pop_front();

    if (!ready->Started) {
        ready->Started = true;
        ready->Stack.Push(0); /* stack address must be aligned before call */
        ready->Context.SetStackPointer(ready->Stack.Pointer());
        ready->Context.SetInstructionPointer(reinterpret_cast<void*>(&TReactor::CoroWrapper));
    }

    TCoroutine* old = CurrentCoro;
    CurrentCoro = ready;

    SPDLOG_DEBUG(Logger_, "switch: {} -> {}", reinterpret_cast<void*>(old), reinterpret_cast<void*>(ready));
    old->Context.SwitchTo(
        ready->Context,
        ready->Stack.Start(), ready->Stack.Size(),
        exitOld
    );

    FinishedCoroutines_.clear();
}

void TReactor::Wakeup(TCoroutine* coro) {
    ASSERT(coro->Reactor == this);

    if (!coro->Awake) {
        ScheduledNextCoroutines_.push_back(coro);
        coro->Awake = true;
    }
}

void TReactor::UpdateWaitState(int fd, uint32_t events, TCoroutine* value) {
    if (events & TReactor::EvRead) {
        ASSERT(!value || !WaitState_[fd].Reader);
        WaitState_[fd].Reader = value;
    }

    if (events & TReactor::EvWrite) {
        ASSERT(!value || !WaitState_[fd].Writer);
        WaitState_[fd].Writer = value;
    }
}

TResult<int> TReactor::WaitFor(int fd, uint32_t waitEvents, TDeadline deadline) {
    ASSERT(fd < static_cast<int>(WaitState_.size()));

    if (CurrentCoro->Canceled) {
        SPDLOG_DEBUG(Logger_, "{} canceled immediately", reinterpret_cast<void*>(CurrentCoro));
        return TResult<int>::MakeCanceled();
    }

    int readyMask = WaitState_[fd].ReadyEvents & waitEvents;
    if (WaitState_[fd].ReadyEvents & waitEvents) {
        SPDLOG_DEBUG(Logger_, "{} woken up immediately", reinterpret_cast<void*>(CurrentCoro));
        return TResult<int>::MakeSuccess(readyMask);
    }

    WaitState_[fd].ReadyEvents &= ~waitEvents;

    UpdateWaitState(fd, waitEvents, CurrentCoro);

    SPDLOG_DEBUG(Logger_, "{} starts WaitFor(fd={}, waitEvents={})", reinterpret_cast<void*>(CurrentCoro), fd, waitEvents);

    if (deadline != TDeadline::max()) {
        CurrentCoro->DeadlineReached = false;
        CurrentCoro->Deadline = deadline;
        DeadlineQueue_.Push(CurrentCoro);
    }

    SwitchCoroutine(false);

    if (CurrentCoro->DeadlineReached) {
        SPDLOG_DEBUG(Logger_, "{} deadline reached while WaitFor(fd={}, waitEvents={})", reinterpret_cast<void*>(CurrentCoro), fd, waitEvents);
        UpdateWaitState(fd, waitEvents, nullptr);
        return TResult<int>::MakeTimedOut();
    }

    if (CurrentCoro->Canceled) {
        SPDLOG_DEBUG(Logger_, "{} canceled in WaitFor", reinterpret_cast<void*>(CurrentCoro));
        UpdateWaitState(fd, waitEvents, nullptr);
        return TResult<int>::MakeCanceled();
    }

    SPDLOG_DEBUG(Logger_, "{} woken up", reinterpret_cast<void*>(CurrentCoro));

    UpdateWaitState(fd, waitEvents, nullptr);

    readyMask = WaitState_[fd].ReadyEvents & waitEvents;
    ASSERT(readyMask != 0);
    return TResult<int>::MakeSuccess(readyMask);
}

void TReactor::OnSignals(TSignalSet signals, TSignalHandler handler) {
    BlockedSignals_ |= signals;
    BlockedSignals_.Block();

    if (SignalFd_ != -1) {
        ::close(SignalFd_);
    }
    SignalFd_ = ::signalfd(-1, &BlockedSignals_.Set(), SFD_NONBLOCK);

    if (SignalFd_ == -1) {
        ThrowErrno("signalfd failed");
    }

    EpollOp(EPOLL_CTL_ADD, SignalFd_, EPOLLIN);

    for (size_t sig = SIGMIN; sig < SIGMAX; sig++) {
        if (signals.Has(sig)) {
            SignalHandlers_[sig] = handler;
        }
    }
}

TResult<size_t> TReactor::Read(int fd, void* to, size_t sz, TDeadline deadline) {
    ASSERT(sz > 0);
    while (true) {
        TResult<int> eventsMask = Reactor()->WaitFor(fd, TReactor::EvRead, deadline);
        if (!eventsMask) {
            return TResult<size_t>::ForwardError(eventsMask);
        }
        ssize_t res = ::read(fd, to, sz);
        if (res == -1) {
            if (errno == EAGAIN) {
                WaitState_[fd].ReadyEvents &= ~TReactor::EvRead;
                continue;
            }
            SPDLOG_DEBUG(Logger_, "{} performed Read({}, {}, {}) = {}", reinterpret_cast<void*>(CurrentCoro), fd, to, sz, -errno);
            return TResult<size_t>::MakeFail(errno);
        } else {
            SPDLOG_DEBUG(Logger_, "{} performed Read({}, {}, {}) = {}", reinterpret_cast<void*>(CurrentCoro), fd, to, sz, res);
            return TResult<size_t>::MakeSuccess(res);
        }
    }
}

TResult<size_t> TReactor::Write(int fd, const void* from, size_t sz, TDeadline deadline) {
    ASSERT(sz > 0);
    while (true) {
        TResult<int> eventsMask = Reactor()->WaitFor(fd, TReactor::EvWrite, deadline);
        if (!eventsMask) {
            return TResult<size_t>::ForwardError(eventsMask);
        }
        ssize_t res = ::write(fd, from, sz);
        if (res == -1) {
            if (errno == EAGAIN) {
                WaitState_[fd].ReadyEvents &= ~TReactor::EvWrite;
                continue;
            }
            SPDLOG_DEBUG(Logger_, "{} performed Read({}, {}, {}) = {}", reinterpret_cast<void*>(CurrentCoro), fd, from, sz, -errno);
            return TResult<size_t>::MakeFail(errno);
        } else {
            SPDLOG_DEBUG(Logger_, "{} performed Write({}, {}, {}) = {}", reinterpret_cast<void*>(CurrentCoro), fd, from, sz, res);
            return TResult<size_t>::MakeSuccess(res);
        }
    }
}

TResult<int> TReactor::Accept(int fd, TSocketAddress* sockAddr, TDeadline deadline) {
    while (true) {
        TResult<int> eventsMask = Reactor()->WaitFor(fd, TReactor::EvRead, deadline);
        if (!eventsMask) {
            return TResult<int>::ForwardError(eventsMask);
        }
        sockaddr_storage addr;
        socklen_t len = sizeof(addr);
        int newFd = ::accept4(fd, reinterpret_cast<sockaddr*>(&addr), &len, SOCK_NONBLOCK);
        if (newFd == -1) {
            if (errno == EAGAIN) {
                WaitState_[fd].ReadyEvents &= ~TReactor::EvRead;
                continue;
            }
            SPDLOG_DEBUG(Logger_, "{} performed Accept({}) = {}", reinterpret_cast<void*>(CurrentCoro), fd, -errno);
            return TResult<int>::MakeFail(errno);
        } else {
            *sockAddr = TSocketAddress(reinterpret_cast<sockaddr*>(&addr), len);
            SPDLOG_DEBUG(Logger_, "{} performed Accept({}) = [ {}, \"{}:{}\" ]", reinterpret_cast<void*>(CurrentCoro), fd, newFd, sockAddr->Host(), sockAddr->Port());
            return TResult<int>::MakeSuccess(newFd);
        }
    }
}

TResult<bool> TReactor::Connect(int fd, const TSocketAddress& addr, TDeadline deadline) {
    int res = ::connect(fd, addr.AddressAs<const sockaddr*>(), addr.Length());
    if (res == -1) {
        if (errno == EINPROGRESS) {
            WaitState_[fd].ReadyEvents &= ~TReactor::EvWrite;

            TResult<int> eventsMask = WaitFor(fd, TReactor::EvWrite, deadline);
            if (!eventsMask) {
                return TResult<bool>::ForwardError(eventsMask);
            }

            if (WaitState_[fd].ReadyEvents & TReactor::EvErr) {
                int err = 0;
                socklen_t errLen = sizeof(err);
                res = getsockopt(fd, SOL_SOCKET, SO_ERROR, &err, &errLen);
                WaitState_[fd].ReadyEvents &= ~TReactor::EvErr;

                SPDLOG_DEBUG(Logger_, "{} performed Connect({}, \"{}:{}\") = {}", reinterpret_cast<void*>(CurrentCoro), fd, addr.Host(), addr.Port(), -err);
                return TResult<bool>::MakeFail(err);
            }
        } else {
            SPDLOG_DEBUG(Logger_, "{} performed Connect({}, \"{}:{}\") = {}", reinterpret_cast<void*>(CurrentCoro), fd, addr.Host(), addr.Port(), -errno);
            return TResult<bool>::MakeFail(errno);
        }
    }

    SPDLOG_DEBUG(Logger_, "{} performed Connect({}, \"{}:{}\") = 0", reinterpret_cast<void*>(CurrentCoro), fd, addr.Host(), addr.Port());
    return TResult<bool>::MakeSuccess(true);
}

int TReactor::EpollOp(int op, int fd, int events) {
    epoll_event ctl_event;
    ctl_event.events = events;
    ctl_event.data.fd = fd;
    return ::epoll_ctl(EpollFd_, op, fd, &ctl_event);
}

void TReactor::DoSignal() {
    for (;;) {
        signalfd_siginfo sigInfo;

        int ret = ::read(SignalFd_, &sigInfo, sizeof(sigInfo));

        if (ret < 0) {
            break;
        }

        int sig = sigInfo.ssi_signo;
        if (SignalHandlers_[sig]) {
            SignalHandlers_[sig](sigInfo);
        }
    }
}

void TReactor::DoPoll() {
    static constexpr size_t MaxEvents = 256;

    epoll_event events[MaxEvents];

    int timeout = -1;

    if (ScheduledCoroutines_.size() != 0) {
        timeout = 0;
    } else if (!DeadlineQueue_.Empty()) {
        if (DeadlineQueue_.Top()->Deadline <= std::chrono::steady_clock::now()) {
            timeout = 0;
        } else {
            timeout = std::chrono::duration_cast<std::chrono::milliseconds>(DeadlineQueue_.Top()->Deadline - std::chrono::steady_clock::now()).count();
        }
    }

    int res = 0;
    while (true) {
        res = ::epoll_wait(EpollFd_, events, MaxEvents, timeout);

        if (res == -1) {
            if (errno == EINTR) {
                continue;
            }
            /* TODO: throw it in initial coroutine */
            ThrowErrno("epoll_wait failed");
        }

        break;
    }

    for (int i = 0; i < res; i++) {
        int fd = events[i].data.fd;

        if (fd == SignalFd_) {
            DoSignal();
        } else {
            if (events[i].events & EPOLLIN) {
                WaitState_[fd].ReadyEvents |= TReactor::EvRead;
                if (WaitState_[fd].Reader) {
                    WaitState_[fd].Reader->Wakeup();
                }
            }

            if (events[i].events & EPOLLOUT) {
                WaitState_[fd].ReadyEvents |= TReactor::EvWrite;
                if (WaitState_[fd].Writer) {
                    WaitState_[fd].Writer->Wakeup();
                }
            }

            if (events[i].events & EPOLLERR) {
                WaitState_[fd].ReadyEvents |= TReactor::EvErr;
            }
        }
    }

    while (!DeadlineQueue_.Empty() && DeadlineQueue_.Top()->Deadline <= std::chrono::steady_clock::now()) {
        TCoroutine* coro = DeadlineQueue_.Peek();
        coro->DeadlineReached = true;
        coro->Wakeup();
    }
}

void TReactor::RegisterNonBlockingFd(int fd) {
    SPDLOG_DEBUG(Logger_, "fd={} registered", fd);
    if (WaitState_.size() <= fd) {
        WaitState_.resize(fd + 1);
    }
    WaitState_[fd].ReadyEvents = 0;
    EpollOp(EPOLL_CTL_ADD, fd, EPOLLIN | EPOLLOUT | EPOLLET);
}

void TReactor::CloseFd(int fd) {
    SPDLOG_DEBUG(Logger_, "fd={} closed", fd);

    if (WaitState_[fd].Writer) {
        WaitState_[fd].Writer->Wakeup();
    }

    if (WaitState_[fd].Reader) {
        WaitState_[fd].Reader->Wakeup();
    }

    WaitState_[fd].Writer = nullptr;
    WaitState_[fd].Reader = nullptr;
    WaitState_[fd].ReadyEvents = 0;
}

void TReactor::Cancel(TCoroutine* coro) {
    SPDLOG_DEBUG(Logger_, "cancel {}", reinterpret_cast<void*>(coro));
    if (coro->PosInDeadlineQueue != TDeadlineQueue::InvalidPos) {
        DeadlineQueue_.Remove(coro);
    }
    coro->Canceled = true;
    coro->Wakeup();
}

void TReactor::CancelAll() {
    for (std::unique_ptr<TCoroutine>& coro : ActiveCoroutines_) {
        coro->Cancel();
    }
}

TResult<bool> TReactor::AwaitAll(std::initializer_list<TCoroutine*> coros) {
    if (CurrentCoro->Canceled) {
        return TResult<bool>::MakeCanceled();
    }

    for (TCoroutine* coro : coros) {
        ASSERT(coro != CurrentCoro);
        ASSERT(!coro->Awaiter);
        coro->Awaiter = CurrentCoro;
    }

    size_t finishedCoroutines = 0;
    while (finishedCoroutines < coros.size()) {
        SwitchCoroutine(false);

        if (CurrentCoro->Canceled) {
            for (TCoroutine* coro : coros) {
                /* prevent use-after-free */
                coro->Awaiter = nullptr;
                coro->Cancel();
                SPDLOG_DEBUG(Logger_, "canceled {} due to canceling {}", reinterpret_cast<void*>(coro), reinterpret_cast<void*>(CurrentCoro));
            }
            return TResult<bool>::MakeCanceled();
        }

        finishedCoroutines++;
    }

    SPDLOG_DEBUG(Logger_, "{} done awaiting", reinterpret_cast<void*>(CurrentCoro));

    return TResult<bool>::MakeSuccess(true);
}

static inline size_t Parent(size_t idx) {
    return (idx - 1) / 2;
}

static inline size_t LeftChild(size_t idx) {
    return idx * 2 + 1;
}

static inline size_t RightChild(size_t idx) {
    return idx * 2 + 2;
}

void TReactor::TDeadlineQueue::SiftUp(size_t idx) {
    while (idx > 0 && Queue_[idx]->Deadline < Queue_[Parent(idx)]->Deadline) {
        Swap(idx, Parent(idx));
        idx = Parent(idx);
    }
}

void TReactor::TDeadlineQueue::SiftDown(size_t idx) {
    while (LeftChild(idx) < Queue_.size()) {
        size_t j = LeftChild(idx);

        if (RightChild(idx) < Queue_.size() &&
            Queue_[RightChild(idx)]->Deadline < Queue_[LeftChild(idx)]->Deadline) {
            j = RightChild(idx);
        }

        if (Queue_[idx]->Deadline <= Queue_[j]->Deadline) {
            break;
        }

        Swap(idx, j);
        idx = j;
    }
}
