#include "http.h"

#include <sstream>

#include <picohttpparser/picohttpparser.h>

enum {
    MAX_HEADERS = 100
};

static std::string CRLF = "\r\n";

TResult<THttpRequest> THttpHandle::ReadRequest(TReactor::TDeadline deadline) {
    TResult<THttpRequest> result;
    size_t prevBufLen = 0;

    TResult<size_t> res = Reader_.ReadUntil([this, &result, &prevBufLen](TMemoryRegion region) {
        const char* method = nullptr;
        const char* path = nullptr;
        size_t methodLen = 0;
        size_t pathLen = 0;
        size_t numHeaders = MAX_HEADERS;
        int minorVersion = 0;
        struct phr_header headers[MAX_HEADERS] = {};

        int res = phr_parse_request(
            region.DataAs<const char*>(), region.Size(),
            &method, &methodLen,
            &path, &pathLen,
            &minorVersion,
            headers, &numHeaders,
            prevBufLen
        );

        prevBufLen = region.Size();

        if (res > 0) {
            THttpRequest request;
            request.Method = std::string(method, methodLen);
            request.Url = std::string(path, pathLen);
            request.MinorVersion = minorVersion;
            request.Headers.reserve(numHeaders);
            for (size_t i = 0; i < numHeaders; i++) {
                request.Headers.emplace(
                    std::string(headers[i].name, headers[i].name_len),
                    std::string(headers[i].value, headers[i].value_len)
                );
            }
            Reader_.ChopBegin(res);
            result = TResult<THttpRequest>::MakeSuccess(std::move(request));
            return true;
        } else if (res == -1) {
            result = TResult<THttpRequest>::MakeFail(-2);
            return true;
        } else {
            ASSERT(res == -2);
        }

        return false;
    }, deadline);

    return result;
}

TResult<THttpResponse> THttpHandle::ReadResponse(TReactor::TDeadline deadline) {
    TResult<THttpResponse> result;
    size_t prevBufLen = 0;

    TResult<size_t> res = Reader_.ReadUntil([this, &result, &prevBufLen](TMemoryRegion region) {
        int minorVersion = 0;
        int status = 0;
        const char* reason = nullptr;
        size_t reasonLen = 0;
        size_t numHeaders = MAX_HEADERS;
        struct phr_header headers[MAX_HEADERS] = {};

        int ret = phr_parse_response(
            region.DataAs<const char*>(), region.Size(),
            &minorVersion,
            &status,
            &reason, &reasonLen,
            headers, &numHeaders,
            prevBufLen
        );

        prevBufLen = region.Size();

        if (ret > 0) {
            THttpResponse response;
            response.Status = status;
            response.Reason = std::string(reason, reasonLen);
            response.MinorVersion = minorVersion;
            response.Headers.reserve(numHeaders);
            for (size_t i = 0; i < numHeaders; i++) {
                response.Headers.emplace(
                    std::string(headers[i].name, headers[i].name_len),
                    std::string(headers[i].value, headers[i].value_len)
                );
            }
            Reader_.ChopBegin(ret);
            result = TResult<THttpResponse>::MakeSuccess(std::move(response));
            return true;
        } else if (ret == -1) {
            result = TResult<THttpResponse>::MakeFail(-2);
            return true;
        } else {
            ASSERT(ret == -2);
        }

        return false;
    }, deadline);

    return result;
}

static std::string MergeHeaders(const THeaders& headers) {
    std::string result;

    size_t resultingSize = 0;
    for (const auto& header : headers) {
        resultingSize += header.first.size();
        resultingSize += 2; // ": "
        resultingSize += header.second.size();
        resultingSize += 2; // "\r\n"
    }
    resultingSize += 2; // "\r\n"

    result.reserve(resultingSize);

    for (const auto& header : headers) {
        result += fmt::format("{}: {}\r\n", header.first, header.second);
    }

    result += "\r\n";

    return result;
}

TResult<size_t> THttpHandle::WriteRequest(const THttpRequest& request, TReactor::TDeadline deadline) {
    TMemoryRegionChain chain;

    std::string requestLine = fmt::format(
        "{} {} HTTP/1.{}\r\n",
        request.Method,
        request.Url,
        request.MinorVersion
    );
    chain.Add(requestLine);

    std::string headers = MergeHeaders(request.Headers);
    chain.Add(headers);

    return Handle_->WriteAll(chain);
}

TResult<size_t> THttpHandle::WriteResponse(const THttpResponse& response, TReactor::TDeadline deadline) {
    TMemoryRegionChain chain;

    std::string responseLine = fmt::format(
        "HTTP/1.{} {} {}\r\n",
        response.MinorVersion,
        response.Status,
        response.Reason
    );
    chain.Add(responseLine);

    std::string headers = MergeHeaders(response.Headers);
    chain.Add(headers);

    return Handle_->WriteAll(chain);
}

TResult<size_t> THttpHandle::TransferBody(THttpHandle& other, const THttpMessage& message, TReactor::TDeadline deadline) {
    auto it = message.Headers.find("Content-Length");

    if (it != message.Headers.end()) {
        std::stringstream ss(it->second);
        size_t size = 0;
        ss >> size;
        return Reader_.TransferExactly(*other.Handle_, size, deadline);
    }

    return TResult<size_t>::MakeSuccess(0);
}

TResult<TMemoryRegion> THttpHandle::ReadBody(const THttpMessage& message, TReactor::TDeadline deadline) {
    auto it = message.Headers.find("Content-Length");

    if (it != message.Headers.end()) {
        std::stringstream ss(it->second);
        size_t size = 0;
        ss >> size;
        return Reader_.ReadExactly(size, deadline);
    }

    return TResult<TMemoryRegion>::MakeSuccess(TMemoryRegion(nullptr, 0));
}
