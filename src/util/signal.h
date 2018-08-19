#pragma once

#include <signal.h>
#include <sys/signalfd.h>

#define SIGMAX 33
#define SIGMIN 0

class TSignalSet {
public:
    TSignalSet() {
        Clear();
    }

    void Add(int sig) {
        ::sigaddset(&Set_, sig);
    }

    void Remove(int sig) {
        ::sigdelset(&Set_, sig);
    }

    void Clear() {
        ::sigemptyset(&Set_);
    }

    void Fill() {
        ::sigfillset(&Set_);
    }

    void operator|=(const TSignalSet& other) {
        ::sigset_t newSet;
        ::sigorset(&newSet, &Set_, &other.Set_);
        Set_ = std::move(newSet);
    }

    friend TSignalSet operator&(const TSignalSet& l, const TSignalSet& r) {
        TSignalSet result;
        ::sigandset(&result.Set_, &l.Set_, &r.Set_);
        return result;
    }

    bool Empty() const {
        return ::sigisemptyset(&Set_);
    }

    bool Has(int sig) {
        TSignalSet set;
        set.Add(sig);
        return !(*this & set).Empty();
    }

    void Block() {
        ::sigprocmask(SIG_BLOCK, &Set_, NULL);
    }

    const ::sigset_t& Set() const {
        return Set_;
    }

private:
    ::sigset_t Set_;
};


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
