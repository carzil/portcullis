#pragma once

#include <memory>
#include <pybind11/pybind11.h>

#include "core/fwd.h"
#include "core/handle.h"

#include "shield/fwd.h"

class TSplicer {
public:
    TSplicer(TContextPtr context, TSocketHandlePtr client, TSocketHandlePtr backend);
    ~TSplicer();

    int Id() const {
        return Id_;
    }

    void End();

private:
    void Transfer();

    TContextPtr Context_;
    TSocketHandlePtr Client_;
    TSocketHandlePtr Backend_;
    TSocketBufferPtr ClientBuffer_;
    TSocketBufferPtr BackendBuffer_;

    bool ClientEof_ = false;
    bool BackendEof_ = false;

    size_t Id_ = 0;
};
