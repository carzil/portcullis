#include "common.h"

#include <vector>

#include <sys/types.h>
#include <sys/uio.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <netinet/ip.h>
#include <sys/socket.h>
#include <netdb.h>

#include <coro/coro.h>
#include <coro/reactor.h>

THandle::THandle(TReactor* reactor, size_t fd)
    : Reactor_(reactor)
    , Fd_(fd)
{
    ASSERT(fd != InvalidFd);
    Reactor_->RegisterNonBlockingFd(fd);
}

void THandle::Close() {
    if (Fd_ != InvalidFd) {
        size_t fd = Fd_;
        ::close(fd);
        Fd_ = -1;
        Reactor_->CloseFd(fd);
    }
}

TResult<size_t> THandle::Read(TMemoryRegion region, TReactor::TDeadline deadline) {
    return Reactor()->Read(Fd(), region.Data(), region.Size(), deadline);
}

TResult<size_t> THandle::Read(TSocketBuffer& to, size_t size, TReactor::TDeadline deadline) {
    size = std::min(size, to.Remaining());
    TResult<size_t> res = Reactor()->Read(Fd(), to.End(), std::min(size, to.Remaining()), deadline);
    if (res) {
        to.Advance(res.Result());
    }
    return res;
}

TResult<size_t> THandle::Write(TMemoryRegion region, TReactor::TDeadline deadline) {
    return Reactor()->Write(Fd(), region.Data(), region.Size(), deadline);
}

TResult<size_t> THandle::Write(TMemoryRegionChain& chain, TReactor::TDeadline deadline) {
    std::list<TMemoryRegion>& regions = chain.Regions();

    std::unique_ptr<iovec[]> iovecs = std::make_unique<iovec[]>(regions.size());

    size_t i = 0;
    for (TMemoryRegion& region : regions) {
        iovecs[i].iov_base = region.Data();
        iovecs[i].iov_len = region.Size();
        i++;
    }

    TResult<size_t> res = Reactor()->Writev(Fd(), iovecs.get(), regions.size());

    if (!res) {
        return TResult<size_t>::ForwardError(res);
    }

    chain.Advance(res.Result());

    return res;
}

TResult<size_t> THandle::WriteAll(TMemoryRegion region, TReactor::TDeadline deadline) {
    size_t written = 0;
    while (!region.Empty()) {
        TResult<size_t> res = Write(region, deadline);
        if (!res) {
            return res;
        }
        region = region.Slice(res.Result());
        written += res.Result();
    }

    return TResult<size_t>::MakeSuccess(written);
}

TResult<size_t> THandle::WriteAll(TMemoryRegionChain& chain, TReactor::TDeadline deadline) {
    size_t written = 0;
    while (!chain.Empty()) {
        TResult<size_t> res = Write(chain, deadline);

        if (!res) {
            return TResult<size_t>::ForwardError(res);
        }

        written += res.Result();
    }

    return TResult<size_t>::MakeSuccess(written);
}

TResult<size_t> THandle::TransferAll(THandle& to, TSocketBuffer& buffer, size_t bytesCount, TReactor::TDeadline deadline) {
    ASSERT(buffer.Empty());

    size_t transfered = 0;

    while (transfered < bytesCount) {
        TResult<size_t> res = Read(buffer, bytesCount - transfered, deadline);

        if (!res) {
            return res;
        }

        if (res.Result() == 0) {
            return TResult<size_t>::MakeSuccess(0);
        }

        res = to.WriteAll(buffer.CurrentMemoryRegion(), deadline);

        if (!res) {
            return res;
        }

        buffer.Reset();

        transfered += res.Result();
    }

    ASSERT(transfered == bytesCount);
    return TResult<size_t>::MakeSuccess(transfered);
}
