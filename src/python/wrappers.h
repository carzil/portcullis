#pragma once

#include <pybind11/pybind11.h>

#include "core/context.h"
#include "core/handle.h"
#include "core/loop.h"
#include "core/buffer.h"

#include "shield/splicer.h"

namespace py = pybind11;

class TSocketHandleWrapper {
public:
    TSocketHandleWrapper(TSocketHandlePtr handle)
        : Handle_(std::move(handle))
    {
    }

    TSocketAddress Address() const {
        return Handle_->Address();
    }

    void Read(py::object handler);
    void Write(std::string, py::object handler);
    void Close();

    TSocketHandlePtr Handle() {
        return Handle_;
    }

private:
    TSocketHandlePtr Handle_;
};

class TContextWrapper {
public:
    TContextWrapper(TContextPtr context)
        : Context_(std::move(context))
    {
    }

    TSocketHandleWrapper MakeTcp() {
        return TSocketHandleWrapper(Context_->Loop->MakeTcp());
    }

    TSplicerPtr Splice(TSocketHandleWrapper in, TSocketHandleWrapper out);
    void Connect(std::string endpointString, py::object handler);

private:
    TContextPtr Context_ = nullptr;
};
