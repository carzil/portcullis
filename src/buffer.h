#pragma once

#include <memory>

#include "utils.h"

class TSocketBuffer : public TMoveOnly {
public:
    TSocketBuffer() = default;

    TSocketBuffer(size_t capacity)
        : Capacity_(capacity)
        , Data_(std::make_unique<uint8_t[]>(capacity))
    {
    }

    size_t Remaining() const {
        return Capacity_ - Size_;
    }

    size_t Size() const {
        return Size_;
    }

    const void* End() const {
        return Data_.get() + Size_;
    }

    void* End() {
        return Data_.get() + Size_;
    }

    void Increase(size_t n) {
        Size_ += n;
    }

    const void* Data() const {
        return Data_.get();
    }

    template<class T>
    const T DataAs() const {
        return static_cast<const T>(Data_.get());
    }

    void Unwind() {
        Size_ = 0;
    }

private:
    size_t Size_ = 0;
    size_t Capacity_ = 0;
    std::unique_ptr<uint8_t[]> Data_ = nullptr;
};
