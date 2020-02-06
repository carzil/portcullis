#pragma once

#include "database.h"

#include <hs/hs_runtime.h>
#include <core/buffer.h>
#include <exception>
#include <functional>


class TStreamRegexpMatcher {
public:
    using TMatchCallback = std::function<bool(unsigned int id, size_t from, size_t to)>;

    TStreamRegexpMatcher(const TStreamRegexpDatabase& db, TMatchCallback cb);

    bool Scan(TMemoryRegion region);

    ~TStreamRegexpMatcher() {
        hs_close_stream(Stream_, Scratch_, nullptr, nullptr);
        hs_free_scratch(Scratch_);
    }

private:
    static int MatchCallback(unsigned int id, unsigned long long from, unsigned long long to, unsigned int flags, void *context);

    hs_stream_t* Stream_ = nullptr;
    hs_scratch_t* Scratch_ = nullptr;
    const TStreamRegexpDatabase& Db_;
    TMatchCallback Cb_;
    std::exception_ptr Exception_;
};
