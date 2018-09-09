#pragma once

#include <string_view>
#include <pybind11/pybind11.h>
#include <core/context.h>
#include <handles/tcp.h>
#include <handles/http.h>

namespace py = pybind11;

class TContextWrapper {
public:
    TContextWrapper(TContextPtr ptr)
        : Context_(ptr)
    {}

    TContextPtr& Context() {
        return Context_;
    }

private:
    TContextPtr Context_;
};

class TPyContextSwitchGuard {
public:
    TPyContextSwitchGuard(TContextWrapper& context)
        : Context_(context)
    {
        State_ = PyThreadState_Swap(nullptr);
    }

    ~TPyContextSwitchGuard() {
        PyThreadState_Swap(State_);
         const auto& internals = py::detail::get_internals();
         PyThread_set_key_value(internals.tstate, State_);
    }

private:
    TContextWrapper& Context_;
    PyThreadState* State_;
};

class TTcpHandleWrapper {
public:
    TTcpHandleWrapper(TContextWrapper context, TTcpHandlePtr handle)
        : Handle_(handle)
        , Context_(context)
    {
    }

    static TTcpHandleWrapper Create(TContextWrapper context, bool ipv6 = false) {
        return TTcpHandleWrapper(context, TTcpHandle::Create(ipv6));
    }

    static TTcpHandleWrapper Connect(TContextWrapper context, TSocketAddress addr);

    py::bytes Read(ssize_t size);
    py::bytes ReadExactly(ssize_t size);
    size_t Write(std::string_view buf);
    void WriteAll(std::string_view buf);

    size_t Transfer(TTcpHandleWrapper other, size_t size);
    size_t TransferAll(TTcpHandleWrapper other);


    void Close() {
        Handle_->Close();
    }

    TTcpHandlePtr Handle() {
        return Handle_;
    }

    TContextWrapper Context() {
        return Context_;
    }

private:
    TTcpHandlePtr Handle_;
    TContextWrapper Context_;
};

class THttpHandleWrapper {
public:
    THttpHandleWrapper(TTcpHandleWrapper wrapper)
        : Handle_(THttpHandle(wrapper.Handle()))
        , Context_(wrapper.Context())
    {}

    THttpRequest ReadRequest() {
        TResult<THttpRequest> res;
        {
            TPyContextSwitchGuard guard(Context_);
            res = Handle_.ReadRequest();
        }

        if (!res) {
            ThrowErr(res.Error(), "read_request failed");
        }

        return res.Result();
    }

    THttpResponse ReadResponse() {
        TResult<THttpResponse> res;
        {
            TPyContextSwitchGuard guard(Context_);
            res = Handle_.ReadResponse();
        }

        if (!res) {
            ThrowErr(res.Error(), "read_request failed");
        }

        return res.Result();
    }

    size_t WriteRequest(const THttpRequest& request) {
        TResult<size_t> res;
        {
            TPyContextSwitchGuard guard(Context_);
            res = Handle_.WriteRequest(request);
        }

        if (!res) {
            ThrowErr(res.Error(), "write_request failed");
        }

        return res.Result();
    }

    size_t WriteResponse(const THttpResponse& response) {
        TResult<size_t> res;
        {
            TPyContextSwitchGuard guard(Context_);
            res = Handle_.WriteResponse(response);
        }


        if (!res) {
            ThrowErr(res.Error(), "write_response failed");
        }

        return res.Result();
    }

    size_t TransferBodyGeneric(THttpHandleWrapper& other, const THttpMessage& message) {
        TResult<size_t> res;
        {
            TPyContextSwitchGuard guard(Context_);
            res = Handle_.TransferBody(other.Handle_, message);
        }

        if (!res) {
            ThrowErr(res.Error(), "transfer_body failed");
        }

        return res.Result();
    }

    size_t TransferBody(THttpHandleWrapper& other, const THttpRequest& request) {
        return TransferBodyGeneric(other, request);
    }

    size_t TransferBody(THttpHandleWrapper& other, const THttpResponse& response) {
        return TransferBodyGeneric(other, response);
    }

    void Close() {
        Handle_.Close();
    }

private:
    THttpHandle Handle_;
    TContextWrapper Context_;
};
