#pragma once

#include <memory>
#include <pybind11/pybind11.h>

#include "core/fwd.h"
#include "core/handle.h"

class TContextWrapper;
class TSocketHandleWrapper;

#include "shield/fwd.h"

class TSplicer {
public:
    TSplicer(TContextWrapper context, TSocketHandleWrapper client, TSocketHandleWrapper backend, py::object onEndCb);
    ~TSplicer();

    int Id() const {
        return Id_;
    }

    void Start();
    void End();

protected:
    void FlushClientBuffer();
    void FlushBackendBuffer();

private:
    void Cleanup();

    TContextPtr Context_;
    TSocketHandlePtr Client_;
    TSocketHandlePtr Backend_;
    TSocketBufferPtr ClientBuffer_;
    TSocketBufferPtr BackendBuffer_;
    py::object OnEndCb_;

    bool Started_ = false;

    bool ClientEof_ = false;
    bool BackendEof_ = false;

    size_t Id_ = 0;
};
