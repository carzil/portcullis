#pragma once

#include <memory>
#include <vector>
#include <list>

#include "util/generic.h"

class TMemoryRegion {
public:
    TMemoryRegion() = default;

    TMemoryRegion(void* data, size_t size)
        : Data_(data)
        , Size_(size)
    {}

    TMemoryRegion(std::string& s)
        : Data_(s.data())
        , Size_(s.size())
    {}

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

    void CopyTo(TMemoryRegion other, size_t size) {
        ASSERT(other.Size() >= size);
        ASSERT(size <= Size());
        ::memcpy(other.Data(), Data(), size);
    }

    void CopyTo(TMemoryRegion other) {
        CopyTo(other, Size());
    }

    bool EqualTo(TMemoryRegion other) const {
        return ::memcmp(Data(), other.Data(), std::min(Size(), other.Size())) == 0;
    }

    TMemoryRegion FitSize(size_t size) const {
        size_t resultSize = std::min(size, Size());
        return TMemoryRegion(reinterpret_cast<char*>(Data_), resultSize);
    }

    TMemoryRegion Slice(size_t offset) const {
        return TMemoryRegion(reinterpret_cast<char*>(Data_) + offset, Size() - offset);
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

    void Add(TMemoryRegion region) {
        Chain_.push_back(region);
    }

    void Advance(size_t bytes) {
        while (!Chain_.empty() && bytes >= Chain_.begin()->Size()) {
            bytes -= Chain_.begin()->Size();
            Chain_.erase(Chain_.begin());
        }

        if (!Chain_.empty()) {
            *Chain_.begin() = Chain_.begin()->Slice(bytes);
        }
    }

    const std::list<TMemoryRegion>& Regions() const {
        return Chain_;
    }

    std::list<TMemoryRegion>& Regions() {
        return Chain_;
    }

    bool Empty() const {
        return Chain_.empty();
    }

private:
    std::list<TMemoryRegion> Chain_;
};

class TSocketBuffer : public TMoveOnly {
public:
    enum {
        DefaultSize = 16384
    };

    TSocketBuffer() = default;

    TSocketBuffer(size_t capacity)
        : Capacity_(capacity)
        , Data_(new uint8_t[capacity])
    {
    }

    void FillWithZero() {
        ::memset(Data(), '\0', Size());
    }

    size_t Remaining() const {
        return Capacity_ - Size_;
    }

    size_t Capacity() const {
        return Capacity_;
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
        ASSERT(n <= Remaining());
        Size_ += n;
    }

    void ChopBegin(size_t n) {
        ASSERT(n > 0);
        ASSERT(n <= Size());
        if (n == Size()) {
            Reset();
        } else {
            ::memmove(Data(), DataAs<const uint8_t*>() + n, Size() - n);
            Size_ -= n;
        }
    }

    inline void Append(const void* data, size_t sz) {
        if (sz > Remaining()) {
            throw TException() << "buffer is full";
        }
        ::memcpy(End(), data, sz);
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
