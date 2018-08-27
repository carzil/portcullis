#pragma once

#include <functional>
#include <cstring>
#include <memory>
#include <netinet/ip.h>
#include <fcntl.h>

#include <coro/reactor.h>
#include <core/buffer.h>
#include <util/exception.h>
#include <util/network/address.h>
#include <util/system.h>


class THandle;
using THandlePtr = std::shared_ptr<THandle>;

class THandle {
protected:
    TReactor* Reactor_ = nullptr;

public:
    THandle(TReactor* reactor, size_t fd);

    virtual ~THandle() {
        Close();
    };

    size_t Fd() const {
        return Fd_;
    }

    bool Active() const {
        return Fd_ != InvalidFd;
    }

    /*
     * Reads no more than `region.Size()` bytes to memory region `region`.
     * @return how many bytes were read.
     */
    TResult<size_t> Read(TMemoryRegion region, TReactor::TDeadline deadline = TReactor::TDeadline::max());

    /*
     * Reads no more than `min(size, buffer.Remaining())` bytes from handle to buffer `to`.
     * @return how many bytes were read.
     */
    TResult<size_t> Read(TSocketBuffer& to, size_t size, TReactor::TDeadline deadline = TReactor::TDeadline::max());

    /*
     * Reads no more than `buffer.Remaining()` bytes from handle to buffer `to`.
     * @return how many bytes were read.
     */
    TResult<size_t> Read(TSocketBuffer& to, TReactor::TDeadline deadline = TReactor::TDeadline::max()) {
        return Read(to, to.Remaining(), deadline);
    }

    /*
     * Writes no more than `region.Size()` bytes from memory region `region`.
     * @return how many bytes were read.
     */
    TResult<size_t> Write(TMemoryRegion region, TReactor::TDeadline deadline = TReactor::TDeadline::max());

    /*
     * Performs single write on head of `chain`.
     * @return how many bytes were written.
     */
    TResult<size_t> Write(TMemoryRegionChain& chain, TReactor::TDeadline deadline = TReactor::TDeadline::max());

    /*
     * Transfers no more than `min(size, buffer.Remaining())` bytes to other handle `to`
     * through empty buffer `buffer`.
     * @return how many bytes were transfered.
     */
    TResult<size_t> Transfer(THandle& to, TSocketBuffer& buffer, size_t size, TReactor::TDeadline deadline = TReactor::TDeadline::max());

    /*
     * Transfers no more than `buffer.Remaining()` bytes to other handle `to`
     * through empty buffer `buffer`.
     * @return how many bytes were transfered.
     */
    TResult<size_t> Transfer(THandle& to, TSocketBuffer& buffer, TReactor::TDeadline deadline = TReactor::TDeadline::max()) {
        return Transfer(to, buffer, buffer.Remaining());
    }

    virtual void Close();

private:
    size_t Fd_ = InvalidFd;
};
