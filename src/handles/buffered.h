#pragma once

#include "common.h"

#include <util/generic.h>

template <typename UnderlyingPtr>
class TBufferedReader : public TMoveOnly {
public:
    TBufferedReader(UnderlyingPtr handle, size_t bufferSize = TSocketBuffer::DefaultSize)
        : Handle_(std::move(handle))
        , Buffer_(bufferSize)
    {}

    TBufferedReader(UnderlyingPtr handle, TSocketBuffer buffer)
        : Handle_(std::move(handle))
        , Buffer_(std::move(buffer))
    {}

    TResult<TMemoryRegion> Read(TReactor::TDeadline deadline = TReactor::TDeadline::max()) {
        if (Buffer_.Empty()) {
            TResult<size_t> res = ReadMore(deadline);
            if (!res) {
                return TResult<TMemoryRegion>::ForwardError(res);
            }
        }

        return TResult<TMemoryRegion>::MakeSuccess(Buffer_.CurrentMemoryRegion());
    }

    TResult<size_t> ReadMore(TReactor::TDeadline deadline = TReactor::TDeadline::max()) {
        if (Buffer_.Full()) {
            return TResult<size_t>::MakeSuccess(0);
        }
        return Handle_->Read(Buffer_, deadline);
    }

    TResult<TMemoryRegion> ReadExactly(size_t size, TReactor::TDeadline deadline = TReactor::TDeadline::max()) {
        if (size > Buffer_.Capacity()) {
            return TResult<TMemoryRegion>::MakeFail(-1);
        }

        while (Buffer_.Size() < size) {
            TResult<size_t> res = Handle_->Read(Buffer_, deadline);

            if (!res) {
                return TResult<TMemoryRegion>::ForwardError(res);
            }

            if (res.Result() == 0) {
                return TResult<TMemoryRegion>::MakeFail(-2);
            }
        }

        return TResult<TMemoryRegion>::MakeSuccess(Buffer_.CurrentMemoryRegion().FitSize(size));
    }

    template <typename Predicate>
    TResult<size_t> ReadUntil(Predicate&& pred, TReactor::TDeadline deadline = TReactor::TDeadline::max()) {
        size_t bytesRead = CurrentMemoryRegion().Size();

        while (true) {
            if (pred(CurrentMemoryRegion())) {
                break;
            }

            if (Full()) {
                return TResult<size_t>::MakeFail(-1);
            }

            TResult<size_t> res = ReadMore(deadline);

            if (!res) {
                return res;
            }

            if (res.Result() == 0) {
                return TResult<size_t>::MakeFail(-2);
            }

            bytesRead += res.Result();
        }

        return TResult<size_t>::MakeSuccess(bytesRead);
    }

    template <typename Other>
    TResult<size_t> TransferExactly(Other& to, size_t maxSize, TReactor::TDeadline deadline = TReactor::TDeadline::max()) {
        size_t transfered = 0;

        if (!Buffer_.Empty()) {
            TResult<size_t> res = to.WriteAll(Buffer_.CurrentMemoryRegion().FitSize(maxSize));
            if (!res) {
                return res;
            }
            transfered = res.Result();
            Buffer_.Reset();
        }

        if (transfered < maxSize) {
            TResult<size_t> res = Handle_->TransferExactly(to, Buffer_, maxSize - transfered, deadline);

            if (!res) {
                return res;
            }

            transfered += res.Result();
        }

        return TResult<size_t>::MakeSuccess(transfered);
    }

    void ChopBegin(size_t size) {
        Buffer_.ChopBegin(size);
    }

    size_t BufferedSize() const {
        return Buffer_.Size();
    }

    TMemoryRegion CurrentMemoryRegion() const {
        return Buffer_.CurrentMemoryRegion();
    }

    bool Full() const {
        return Buffer_.Full();
    }

private:
    UnderlyingPtr Handle_;
    TSocketBuffer Buffer_;
};
