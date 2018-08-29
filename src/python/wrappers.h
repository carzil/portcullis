#pragma once

#include <string_view>
#include <pybind11/pybind11.h>
#include <core/context.h>
#include <coro/tcp_handle.h>

namespace py = pybind11;

class TTcpHandleWrapper {
public:
    TTcpHandleWrapper(TContextPtr context, TTcpHandlePtr handle)
        : Handle_(handle)
        , Context_(context)
    {
    }

    static TTcpHandleWrapper Create(TContextPtr context, bool ipv6 = false) {
        return TTcpHandleWrapper(context, TTcpHandle::Create(ipv6));
    }

    static TTcpHandleWrapper Connect(TContextPtr context, TSocketAddress addr);

    py::bytes Read(ssize_t size);
    py::bytes ReadExactly(ssize_t size);
    size_t Write(std::string_view buf);
    void WriteAll(std::string_view buf);

    size_t Transfer(TTcpHandleWrapper other, size_t size);
    size_t TransferAll(TTcpHandleWrapper other);


    void Close() {
        Handle_->Close();
    }

private:
    TTcpHandlePtr Handle_;
    TContextPtr Context_;
};

