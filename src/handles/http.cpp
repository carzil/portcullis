#include "http.h"

#include <sstream>

#include <picohttpparser/picohttpparser.h>

enum {
    MAX_HEADERS = 100
};

static std::string CRLF = "\r\n";

TResult<THttpRequest> THttpHandle::ReadRequest(TReactor::TDeadline deadline) {
    while (true) {
        if (Buffer_.Full()) {
            // TODO: error codes
            return TResult<THttpRequest>::MakeFail(-1);
        }

        size_t prevBufLen = Buffer_.Size();

        TResult<size_t> res = Handle_->Read(Buffer_, deadline);

        if (!res) {
            return TResult<THttpRequest>::ForwardError(res);
        }

        if (res.Result() == 0) {
            return TResult<THttpRequest>::MakeFail(-1);
        }

        const char* method = nullptr;
        const char* path = nullptr;
        size_t methodLen = 0;
        size_t pathLen = 0;
        size_t numHeaders = MAX_HEADERS;
        int minorVersion = 0;
        struct phr_header headers[MAX_HEADERS] = {};

        int ret = phr_parse_request(
            Buffer_.DataAs<const char*>(), Buffer_.Size(),
            &method, &methodLen,
            &path, &pathLen,
            &minorVersion,
            headers, &numHeaders,
            prevBufLen
        );

        if (ret > 0) {
            THttpRequest result;
            result.Method = std::string(method, methodLen);
            result.Url = std::string(path, pathLen);
            result.MinorVersion = minorVersion;
            result.Headers.reserve(numHeaders);
            for (size_t i = 0; i < numHeaders; i++) {
                result.Headers.emplace(
                    std::string(headers[i].name, headers[i].name_len),
                    std::string(headers[i].value, headers[i].value_len)
                );
            }
            Buffer_.ChopBegin(ret);
            return TResult<THttpRequest>::MakeSuccess(std::move(result));
        } else if (ret == -1) {
            return TResult<THttpRequest>::MakeFail(-2);
        } else {
            ASSERT(ret == -2);
        }
    }
}

TResult<THttpResponse> THttpHandle::ReadResponse(TReactor::TDeadline deadline) {
    while (true) {
        if (Buffer_.Full()) {
            // TODO: error codes
            return TResult<THttpResponse>::MakeFail(-1);
        }

        size_t prevBufLen = Buffer_.Size();

        TResult<size_t> res = Handle_->Read(Buffer_, deadline);

        if (!res) {
            return TResult<THttpResponse>::ForwardError(res);
        }

        if (res.Result() == 0) {
            return TResult<THttpResponse>::MakeFail(-1);
        }

        size_t numHeaders = MAX_HEADERS;
        int minorVersion = 0;
        int status = 0;
        const char* reason = nullptr;
        size_t reasonLen = 0;
        struct phr_header headers[MAX_HEADERS] = {};

        int ret = phr_parse_response(
            Buffer_.DataAs<const char*>(), Buffer_.Size(),
            &minorVersion,
            &status,
            &reason, &reasonLen,
            headers, &numHeaders,
            prevBufLen
        );

        if (ret > 0) {
            THttpResponse result;
            result.Status = status;
            result.Reason = std::string(reason, reasonLen);
            result.MinorVersion = minorVersion;
            result.Headers.reserve(numHeaders);
            for (size_t i = 0; i < numHeaders; i++) {
                result.Headers.emplace(
                    std::string(headers[i].name, headers[i].name_len),
                    std::string(headers[i].value, headers[i].value_len)
                );
            }
            Buffer_.ChopBegin(ret);
            return TResult<THttpResponse>::MakeSuccess(std::move(result));
        } else if (ret == -1) {
            return TResult<THttpResponse>::MakeFail(-2);
        } else {
            ASSERT(ret == -2);
        }
    }
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

TResult<size_t> THttpHandle::TransferAll(THttpHandle& other, size_t size, TReactor::TDeadline deadline) {
    TMemoryRegion region = Buffer_.CurrentMemoryRegion();

    TResult<size_t> res = other.Handle_->WriteAll(region, deadline);
    if (!res) {
        return res;
    }

    size_t transfered = res.Result();

    if (transfered == size) {
        return res;
    }

    Buffer_.Reset();
    res = Handle_->TransferAll(*other.Handle_, Buffer_, size - transfered, deadline);

    if (!res) {
        return TResult<size_t>::ForwardError(res);
    }

    transfered += res.Result();

    ASSERT(transfered == size);
    return TResult<size_t>::MakeSuccess(transfered);
};

TResult<size_t> THttpHandle::TransferBody(THttpHandle& other, const THttpMessage& message, TReactor::TDeadline deadline) {
    auto it = message.Headers.find("Content-Length");

    if (it != message.Headers.end()) {
        std::stringstream ss(it->second);
        size_t size = 0;
        ss >> size;
        return TransferAll(other, size, deadline);
    }

    return TResult<size_t>::MakeSuccess(0);
}
