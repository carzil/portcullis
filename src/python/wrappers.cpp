#include "wrappers.h"
#include <handles/buffered.h>

py::bytes TTcpHandleWrapper::Read(ssize_t size) {
    TResult<TMemoryRegion> res;
    {
        TPyContextSwitchGuard guard(Context_);
        res = Reader_->Read();
    }

    if (!res) {
        ThrowErr(res.Error(), "read failed");
    }
    const TMemoryRegion& region = res.Result();
    py::bytes result(region.DataAs<const char*>(), region.Size());
    Reader_->ChopBegin(region.Size());
    return result;
}

py::bytes TTcpHandleWrapper::ReadExactly(ssize_t size) {
    TResult<TMemoryRegion> res;
    {
        TPyContextSwitchGuard guard(Context_);
        res = Reader_->ReadExactly(size);
    }

    if (!res) {
        ThrowErr(res.Error(), "read_exactly failed");
    }

    py::bytes result(res.Result().DataAs<const char*>(), res.Result().Size());
    Reader_->ChopBegin(res.Result().Size());
    return result;
}

size_t TTcpHandleWrapper::Write(std::string_view buf) {
    TMemoryRegion region(const_cast<char*>(buf.data()), buf.size());

    TResult<size_t> res;
    {
        TPyContextSwitchGuard guard(Context_);
        res = Handle_->Write(region);
    }
    if (!res) {
        ThrowErr(res.Error(), "write failed");
    }
    return res.Result();
}

void TTcpHandleWrapper::WriteAll(std::string_view buf) {
    TResult<size_t> res;
    {
        TPyContextSwitchGuard guard(Context_);
        res = Handle_->WriteAll({ const_cast<char*>(buf.data()), buf.size() });
    }
    if (!res) {
        ThrowErr(res.Error(), "write failed");
    }
}

TTcpHandleWrapper TTcpHandleWrapper::Connect(TContextWrapper context, TSocketAddress addr) {
    TTcpHandlePtr handle = TTcpHandle::Create(addr.Ipv6());
    TResult<bool> res;
    {
        TPyContextSwitchGuard guard(context);
        res = handle->Connect(addr);
    }
    if (!res) {
        ThrowErr(res.Error(), "connect failed");
    }
    return TTcpHandleWrapper(std::move(context), std::move(handle));
}

size_t TTcpHandleWrapper::TransferExactly(TTcpHandleWrapper other, size_t size) {
    TResult<size_t> res;
    {
        TPyContextSwitchGuard guard(Context_);
        res = Reader_->TransferExactly(*other.Handle_, size);
    }

    if (!res) {
        ThrowErr(res.Error(), "transfer_all failed");
    }

    return size;
}
