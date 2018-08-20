#pragma once

#include <vector>
#include <list>
#include <memory>

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
        bool Started = false;
        bool Awake = false;
        bool Canceled = false;

        TCoroutine* Awaiter = nullptr;

        /* Iterator in ActiveCoroutines_ list for fast removal */
        std::list<std::unique_ptr<TCoroutine>>::iterator ListIter;
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
    int AwaitAll(std::initializer_list<TCoroutine*> coros);

    /*
     * Synonym for `AwaitAll({ coro })`;
     */
    int Await(TCoroutine* coro) {
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
    TResult<int> WaitFor(int fd, uint32_t events);

    /*
     * Reads at most `size` bytes to memory pointer by `to`.
     * @return how many bytes were read.
     */
    TResult<size_t> Read(int fd, void* to, size_t size);

    /*
     * Writes at most `size` bytes from memory pointer by `from`.
     * @return how many bytes were written.
     */
    TResult<size_t> Write(int fd, const void* from, size_t size);

    /*
     * Accepts new connection on given `fd`.
     * @param addr -- address of new connection on success.
     * @return file descriptor of new connection.
     */
    TResult<int> Accept(int fd, TSocketAddress* addr);

    /*
     * Connects given `fd` to endpoint `addr`.
     */
    TResult<bool> Connect(int fd, const TSocketAddress& addr);

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

    std::shared_ptr<spdlog::logger> Logger_;
};

using TCoroutine = TReactor::TCoroutine;

/*
 * Returns current reactor.
 */
TReactor* Reactor();
