#include "wrappers.h"


py::bytes TTcpHandleWrapper::Read(ssize_t size) {
    TSocketBuffer buffer(size);
    TResult<size_t> res = Handle_->Read(buffer);
    if (!res) {
        ThrowErr(res.Error(), "read failed");
    }
    return py::bytes(buffer.DataAs<const char*>(), size);
}

py::bytes TTcpHandleWrapper::ReadExactly(ssize_t size) {
    TSocketBuffer buffer(size);

    while (buffer.Remaining() > 0) {
        TResult<size_t> res = Handle_->Read(buffer);
        if (!res) {
            ThrowErr(res.Error(), "read_exactly failed");
        } else if (res.Result() == 0) {
            throw TException() << "read_exactly failed: eof reached";
        }
    }

    return py::bytes(buffer.DataAs<const char*>(), size);
}

size_t TTcpHandleWrapper::Write(std::string_view buf) {
    TMemoryRegion region(const_cast<char*>(buf.data()), buf.size());

    TResult<size_t> res = Handle_->Write(region);
    if (!res) {
        ThrowErr(res.Error(), "write failed");
    }
    return res.Result();
}

void TTcpHandleWrapper::WriteAll(std::string_view buf) {
    TMemoryRegion region(const_cast<char*>(buf.data()), buf.size());

    while (!region.Empty()) {
        TResult<size_t> res = Handle_->Write(region);
        if (!res) {
            ThrowErr(res.Error(), "write failed");
        }
        region = region.Slice(res.Result());

    }
}

TTcpHandleWrapper TTcpHandleWrapper::Connect(TContextPtr context, TSocketAddress addr) {
    TTcpHandlePtr handle = TTcpHandle::Create(addr.Ipv6());
    TResult<bool> res = handle->Connect(addr);
    if (!res) {
        ThrowErr(res.Error(), "connect failed");
    }
    return TTcpHandleWrapper(std::move(context), std::move(handle));
}

size_t TTcpHandleWrapper::Transfer(TTcpHandleWrapper other, size_t size) {
    /* TODO: allocate in pool */
    TSocketBuffer buffer(4096);

    size_t transfered = 0;
    while (transfered < size) {
        TResult<size_t> res = Handle_->Transfer(*other.Handle_, buffer, std::min(buffer.Remaining(), size));
        if (!res) {
            ThrowErr(res.Error(), "transfer failed");
        }

        if (res.Result() == 0) {
            throw TException() << "transfer failed: eof reached";
        }

        transfered += res.Result();
    }

    return transfered;
}

size_t TTcpHandleWrapper::TransferAll(TTcpHandleWrapper other) {
    /* TODO: allocate in pool */
    TSocketBuffer buffer(4096);

    size_t transfered = 0;
    while (true) {
        TResult<size_t> res = Handle_->Transfer(*other.Handle_, buffer, buffer.Remaining());
        if (!res) {
            ThrowErr(res.Error(), "transfer failed");
        }

        if (res.Result() == 0) {
            break;
        }

        transfered += res.Result();
    }

    return transfered;
}
