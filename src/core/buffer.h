#pragma once

#include <memory>
#include <vector>

#include "util/generic.h"

class TMemoryRegion {
public:
    TMemoryRegion() = default;
    TMemoryRegion(void* data, size_t size)
        : Data_(data)
        , Size_(size)
    {
    }

    const void* Data() const {
        return Data_;
    }

    void* Data() {
        return Data_;
    }

    template <typename T>
    T DataAs() const {
        return static_cast<T>(Data_);
    }

    size_t Size() const {
        return Size_;
    }

    bool Empty() const {
        return Size_ == 0;
    }

    TMemoryRegion Slice(size_t offset) const {
        return TMemoryRegion(reinterpret_cast<char*>(Data_) + offset, Size_ - offset);
    }

private:
    void* Data_ = nullptr;
    size_t Size_ = 0;
};

class TMemoryRegionChain {
public:
    TMemoryRegionChain() = default;
    TMemoryRegionChain(std::initializer_list<TMemoryRegion> chain)
        : Chain_(chain)
    {
    }

    void Advance(size_t bytes) {
        PosInCurrRegion_ += bytes;
        if (PosInCurrRegion_ == Chain_[CurrRegion_].Size()) {
            CurrRegion_++;
            PosInCurrRegion_ = 0;
        }
    }

    TMemoryRegion CurrentMemoryRegion() {
        if (CurrRegion_ >= Chain_.size()) {
            return TMemoryRegion();
        }
        return Chain_[CurrRegion_].Slice(PosInCurrRegion_);
    }

private:
    size_t CurrRegion_ = 0;
    size_t PosInCurrRegion_ = 0;
    std::vector<TMemoryRegion> Chain_;
};

class TSocketBuffer : public TMoveOnly {
public:
    TSocketBuffer() = default;

    TSocketBuffer(size_t capacity)
        : Capacity_(capacity)
        , Data_(new uint8_t[capacity])
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

    void Advance(size_t n) {
        Size_ += n;
    }

    inline void Append(const void* data, size_t sz) {
        if (sz > Remaining()) {
            throw TException() << "buffer is full";
        }
        memcpy(End(), data, sz);
        Advance(sz);
    }

    inline void Append(const std::string& data) {
        Append(data.c_str(), data.size());
    }

    inline void Append(char ch) {
        Append(&ch, 1);
    }

    const void* Data() const {
        return Data_.get();
    }

    void* Data() {
        return Data_.get();
    }

    TMemoryRegion BackwardSlice(size_t size) {
        return TMemoryRegion(DataAs<uint8_t*>() - size, size);
    }

    template<class T>
    const T DataAs() const {
        return reinterpret_cast<T>(Data_.get());
    }

    void Reset() {
        Size_ = 0;
    }

    bool Full() const {
        return Size_ == Capacity_;
    }

    bool Empty() const {
        return Size_ == 0;
    }

    TMemoryRegion CurrentMemoryRegion() const {
        return TMemoryRegion(Data_.get(), Size_);
    }

private:
    size_t Size_ = 0;
    size_t Capacity_ = 0;
    std::unique_ptr<uint8_t[]> Data_ = nullptr;
};

using TSocketBufferPtr = std::shared_ptr<TSocketBuffer>;
