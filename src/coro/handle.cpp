#include "handle.h"

#include <vector>

#include <sys/types.h>
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

TResult<size_t> THandle::Read(TMemoryRegion region) {
    return Reactor()->Read(Fd(), region.Data(), region.Size());
}

TResult<size_t> THandle::Read(TSocketBuffer& to, size_t size) {
    size = std::min(size, to.Remaining());
    TResult<size_t> res = Reactor()->Read(Fd(), to.End(), to.Remaining());
    if (res) {
        to.Advance(res.Result());
    }
    return res;
}

TResult<size_t> THandle::Write(TMemoryRegion region) {
    return Reactor()->Write(Fd(), region.Data(), region.Size());
}

TResult<size_t> THandle::Write(TMemoryRegionChain& chain) {
    TResult<size_t> res = Write(chain.CurrentMemoryRegion());
    if (res) {
        chain.Advance(res.Result());
    }
    return res;
}

TResult<size_t> THandle::Transfer(THandle& to, TSocketBuffer& buffer, size_t bytesCount) {
    ASSERT(buffer.Empty());
    TResult<size_t> res = Read(buffer, bytesCount);

    if (!res) {
        return res;
    }

    if (res.Result() == 0) {
        return TResult<size_t>::MakeSuccess(0);
    }

    buffer.Advance(res.Result());

    TMemoryRegion region = buffer.CurrentMemoryRegion();
    while (!region.Empty()) {
        res = to.Write(region);
        if (!res) {
            return res;
        }
        region = region.Slice(res.Result());
    }

    buffer.Reset();

    return TResult<size_t>::MakeSuccess(res.Result());
}
