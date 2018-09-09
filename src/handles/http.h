#pragma once

#include "buffered.h"
#include "tcp.h"

#include <util/generic.h>

#include <vector>
#include <string>

using THeaders = std::unordered_map<std::string, std::string>;

class THttpMessage {
public:
    THttpMessage() = default;

    THttpMessage(THeaders headers, int major, int minor)
        : Headers(std::move(headers))
        , MajorVersion(major)
        , MinorVersion(minor)
    {}

    THeaders Headers;
    int MajorVersion = 0;
    int MinorVersion = 0;
};

class THttpRequest : public THttpMessage {
public:
    std::string Method;
    std::string Url;
};


class THttpResponse : public THttpMessage {
public:
    int Status = 0;
    std::string Reason;
};

class THttpHandle {
public:
    THttpHandle(TTcpHandlePtr handle, size_t bufferSize = TSocketBuffer::DefaultSize)
        : Handle_(std::move(handle))
        , Buffer_(bufferSize)
    {}

    TResult<THttpRequest> ReadRequest(TReactor::TDeadline deadline = TReactor::TDeadline::max());
    TResult<THttpResponse> ReadResponse(TReactor::TDeadline deadline = TReactor::TDeadline::max());

    TResult<size_t> WriteRequest(const THttpRequest& request, TReactor::TDeadline deadline = TReactor::TDeadline::max());
    TResult<size_t> WriteResponse(const THttpResponse& response, TReactor::TDeadline deadline = TReactor::TDeadline::max());

    TResult<size_t> TransferAll(THttpHandle& other, size_t size, TReactor::TDeadline deadline = TReactor::TDeadline::max());
    TResult<size_t> TransferBody(THttpHandle& other, const THttpMessage& message, TReactor::TDeadline deadline = TReactor::TDeadline::max());

    void Close() {
        Handle_->Close();
    }

private:
    TTcpHandlePtr Handle_;
    TSocketBuffer Buffer_;
};

using THttpHandlePtr = std::shared_ptr<THttpHandle>;
