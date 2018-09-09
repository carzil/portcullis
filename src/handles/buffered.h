#pragma once

#include "common.h"


template <typename Underlying>
class TBufferedHandle {
public:
    TBufferedHandle(Underlying handle, int bufferSize)
        : Handle_(std::move(handle))
        , Buffer_(bufferSize)
    {}

    TBufferedHandle(Underlying handle, TSocketBuffer buffer)
        : Handle_(std::move(handle))
        , Buffer_(std::move(buffer))
    {}

    /*
     * Performs unbuffered read.
     */
    TResult<size_t> ReadDirect(TReactor::TDeadline deadline = TReactor::TDeadline::max()) {
        return Handle_.Read(Buffer_, deadline);
    }

    TMemoryRegion CurrentMemoryRegion() {
        return Buffer_.CurrentMemoryRegion();
    }

    void ChopBegin(size_t size) {
        Buffer_.ChopBegin(size);
    }

private:
    Underlying& Handle_;
    TSocketBuffer Buffer_;
};
