#pragma once

#include <functional>
#include <list>
#include <memory>

#include <coro/context.h>
#include <util/generic.h>

class TCoroStack {
public:
    TCoroStack() = default;
    TCoroStack(size_t stackSize);

    inline void* Pointer() const {
        return reinterpret_cast<uint8_t*>(Start_) + Size_ - Pos_;
    }

    inline void* Start() const {
        return Start_;
    }

    inline size_t Size() const {
        return Size_;
    }

    void Push(uint64_t val);

    ~TCoroStack();

private:
    void* Start_ = nullptr;
    size_t Size_ = 0;
    size_t Pos_ = 0;
};
