#pragma once

#include <vector>
#include <list>
#include <memory>
#include <chrono>

#include <spdlog/spdlog.h>

#include <coro/context.h>
#include <coro/coro.h>
#include <util/system.h>
#include <util/signal.h>
#include <util/network/address.h>

using TSignalHandler = std::function<void(TSignalInfo info)>;

class TReactor : TMoveOnly {
public:
    using TCoroEntry = std::function<void()>;

    /*
     * Event types used in `WaitFor`.
     */
    enum {
        EvRead = 1,
        EvWrite = 1 << 1,
        EvErr = 1 << 2,
    };

    using TDeadline = std::chrono::steady_clock::time_point;

    /*
     * Represents coroutine inside reactor.
     */
    class TCoroutine : TMoveOnly {
    public:
        TCoroutine() = default;
        TCoroutine(TCoroutine&&) = default;

        TCoroutine(TReactor* reactor, TCoroEntry entry)
            : Reactor(reactor)
            , Entry(std::move(entry))
            /* TODO: pass stack size in arguments */
            , Stack(4096 * 100)
        {
        }

        void Wakeup() {
            Reactor->Wakeup(this);
        }

        void Cancel() {
            Reactor->Cancel(this);
        }

        void Await() {
            Reactor->Await(this);
        }

        TReactor* Reactor = nullptr;
        TCoroEntry Entry;
        TCoroStack Stack;
        TSavedContext Context;

        /* TODO: flags */
        bool Started = false;
        bool Awake = false;
        bool Canceled = false;
        bool DeadlineReached = false;

        TCoroutine* Awaiter = nullptr;

        /* for deadline queue */
        TDeadline Deadline;
        size_t PosInDeadlineQueue = -1;

        /* Iterator in ActiveCoroutines_ list for fast removal */
        std::list<std::unique_ptr<TCoroutine>>::iterator ListIter;
    };

    class TDeadlineQueue {
    public:
        static constexpr size_t InvalidPos = -1;

        TCoroutine* Top() const {
            return Queue_[0];
        }

        TCoroutine* Peek() {
            TCoroutine* res = Queue_[0];
            Pop();
            return res;
        }

        void Pop() {
            if (Queue_.size() == 1) {
                Queue_.resize(0);
                return;
            }

            Remove(Top());
        }

        void Push(TCoroutine* coro) {
            Queue_.push_back(coro);
            SiftUp(Queue_.size() - 1);
        }

        void Remove(TCoroutine* coro) {
            ASSERT(coro->PosInDeadlineQueue != InvalidPos);
            Queue_[coro->PosInDeadlineQueue] = Queue_.back();
            Queue_.resize(Queue_.size() - 1);
            SiftDown(0);
        }

        bool Empty() const {
            return Queue_.empty();
        }

    private:
        inline void Swap(size_t l, size_t r) {
            ASSERT(l < Queue_.size() && r < Queue_.size());

            TCoroutine* tmp = Queue_[l];
            Queue_[l] = Queue_[r];
            Queue_[r] = tmp;

            Queue_[l]->PosInDeadlineQueue = l;
            Queue_[r]->PosInDeadlineQueue = r;
        }

        void SiftDown(size_t idx);
        void SiftUp(size_t idx);

        std::vector<TCoroutine*> Queue_;
    };

    TReactor(std::shared_ptr<spdlog::logger> logger);

    /*
     * Schedules execution of coroutine.
     * @param entry -- coroutine's entry point.
     */
    TCoroutine* StartCoroutine(TCoroEntry);

    /*
     * Wait for finish of all coroutines in `coros`.
     * If coroutine canceled while awaiting awaited coroutines are also canceled.
     * @return 0 on success, error code otherwise.
     */
    TResult<bool> AwaitAll(std::initializer_list<TCoroutine*> coros);

    /*
     * Synonym for `AwaitAll({ coro })`;
     */
    TResult<bool> Await(TCoroutine* coro) {
        return AwaitAll({ coro });
    }

    /*
     * After call of this method reactor will run,
     * until there are not finished coroutines.
     */
    void Run();

    /*
     * Each file descriptor must be register before use in Reactor.
     * @param fd -- fd to register.
     */
    void RegisterNonBlockingFd(int fd);

    /*
     * Unregister file descriptor from reactor.
     * @param fd -- fd to unregister.
     */
    void CloseFd(int fd);

    /*
     * Wake up the coroutine. If coroutine sleeps it is scheduled to execution,
     * otherwise function has no effect.
     */
    void Wakeup(TCoroutine* coro);

    /*
     * Cancels the coroutine. Coroutine cannot sleep no more once it was canceled.
     * All blocking operation in canceled coroutine will be immediately interrupted
     * with error code ECANCELLED.
     */
    void Cancel(TCoroutine* coro);

    /*
     * Cancels all coroutines.
     */
    void CancelAll();

    /*
     * Returns current running coroutine in this reactor.
     */
    TCoroutine* Current() const;

    /*
     * Waits until given fd is ready for any of given events.
     * @param fd -- fd to wait.
     * @param events -- events list to wait.
     * @returns ready event mask or integer < 0 if error occurred.
     */
    TResult<int> WaitFor(int fd, uint32_t events, TDeadline deadline = TDeadline::max());

    /*
     * Reads at most `size` bytes to memory pointer by `to`.
     * @return how many bytes were read.
     */
    TResult<size_t> Read(int fd, void* to, size_t size, TDeadline deadline = TDeadline::max());

    /*
     * Writes at most `size` bytes from memory pointer by `from`.
     * @return how many bytes were written.
     */
    TResult<size_t> Write(int fd, const void* from, size_t size, TDeadline deadline = TDeadline::max());

    /*
     * Accepts new connection on given `fd`.
     * @param addr -- address of new connection on success.
     * @return file descriptor of new connection.
     */
    TResult<int> Accept(int fd, TSocketAddress* addr, TDeadline deadline = TDeadline::max());

    /*
     * Connects given `fd` to endpoint `addr`.
     */
    TResult<bool> Connect(int fd, const TSocketAddress& addr, TDeadline deadline = TDeadline::max());

    /*
     * Registers signal handler.
     */
    void OnSignals(TSignalSet sigs, TSignalHandler handler);
    void OnSignal(int sig, TSignalHandler handler) {
        TSignalSet sigs;
        sigs.Add(sig);
        OnSignals(sigs, std::move(handler));
    }

    /*
     * Transfer execution to next scheduled coroutine.
     */
    void Yield();

private:
    struct TWaitState {
        uint32_t ReadyEvents = 0;
        TCoroutine* Writer;
        TCoroutine* Reader;
    };

    void UpdateWaitState(int fd, uint32_t events, TCoroutine* value);

    void Finish(TCoroutine*);

    void SwitchCoroutine();
    void DoSignal();
    void DoPoll();

    int EpollOp(int op, int fd, int events);

    static void CoroWrapper();

    std::list<std::unique_ptr<TCoroutine>> ActiveCoroutines_;
    std::list<TCoroutine*> ScheduledCoroutines_;
    std::list<TCoroutine*> ScheduledNextCoroutines_;
    std::list<std::unique_ptr<TCoroutine>> FinishedCoroutines_;
    TCoroutine InitialCoro_;

    ssize_t EpollFd_ = InvalidFd;

    std::vector<TWaitState> WaitState_;

    int SignalFd_ = -1;
    TSignalSet BlockedSignals_;
    std::array<TSignalHandler, SIGMAX> SignalHandlers_;

    TDeadlineQueue DeadlineQueue_;

    std::shared_ptr<spdlog::logger> Logger_;
};

using TCoroutine = TReactor::TCoroutine;

/*
 * Returns current reactor.
 */
TReactor* Reactor();
