#include "matchers.h"

int TStreamRegexpMatcher::MatchCallback(unsigned int id, unsigned long long from, unsigned long long to, unsigned int flags, void *context) {
    TStreamRegexpMatcher* matcher = reinterpret_cast<TStreamRegexpMatcher*>(context);
    try {
        return matcher->Cb_(id, from, to);
    } catch (...) {
        matcher->Exception_ = std::current_exception();
    }
    return 1;
}

bool TStreamRegexpMatcher::Scan(TMemoryRegion region) {
    Exception_ = nullptr;

    hs_error_t res = hs_scan_stream(Stream_, region.DataAs<const char*>(), region.Size(), 0, Scratch_, &TStreamRegexpMatcher::MatchCallback, this);

    if (res != HS_SUCCESS && res != HS_SCAN_TERMINATED) {
        throw TException() << "failed to parse stream";
    }

    if (Exception_) {
        std::rethrow_exception(Exception_);
    }

    return res == HS_SCAN_TERMINATED;
}

TStreamRegexpMatcher::TStreamRegexpMatcher(const TStreamRegexpDatabase& db, TStreamRegexpMatcher::TMatchCallback cb)
    : Db_(db)
    , Cb_(cb)
{
    hs_error_t res = hs_open_stream(Db_.Ptr(), 0, &Stream_);

    if (res != HS_SUCCESS) {
        throw TException() << "cannot open new Hyperscan stream: " << res;
    }

    res = hs_alloc_scratch(Db_.Ptr(), &Scratch_);

    if (res != HS_SUCCESS) {
        throw TException() << "cannot allocate Hyperscan scratch: " << res;
    }
}
