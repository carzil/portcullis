#include <coro/reactor.h>
#include <handles/tcp.h>
#include <handles/http.h>

#include <gtest/gtest.h>

/* TODO: randomize port */

class HttpHandleTest : public ::testing::Test {
public:
    HttpHandleTest()
        : Reactor_(spdlog::get("reactor"))
    {}

    void CreateConnectedPair(TTcpHandlePtr& a, TTcpHandlePtr& b) {
        TTcpHandlePtr listener = TTcpHandle::Create(false);
        TSocketAddress bindAddr = GetAddrInfo("", "31337", true, "tcp")[0];
        listener->ReuseAddr();
        listener->Bind(bindAddr);
        listener->Listen(1);
        a = TTcpHandle::Create(false);
        Reactor_.StartCoroutine([this, &a, bindAddr]() {
            ASSERT_TRUE(a->Connect(bindAddr));
        });

        TResult<TTcpHandlePtr> accepted = listener->Accept();
        ASSERT_TRUE(accepted);

        b = std::move(accepted.Result());
    }

    void SetUp() override {
        Reactor_.StartCoroutine([this]() {
            CreateConnectedPair(FromClient_, FromServer_);
        });

        Reactor_.Run();
    }

    TReactor Reactor_;
    TTcpHandlePtr FromServer_;
    TTcpHandlePtr FromClient_;
};

TEST_F(HttpHandleTest, GetSimpleRequestTest) {
    Reactor_.StartCoroutine([this]() {
        std::string str = "GET / HTTP/1.1\r\n\r\n";
        FromClient_->WriteAll(str);
    });

    Reactor_.StartCoroutine([this]() {
        THttpHandle httpHandle(FromServer_);
        TResult<THttpRequest> res = httpHandle.ReadRequest();
        ASSERT_TRUE(res);
        const THttpRequest& req = res.Result();
        ASSERT_EQ(req.Url, "/");
        ASSERT_EQ(req.Method, "GET");
        ASSERT_EQ(req.MinorVersion, 1);
        ASSERT_EQ(req.Headers.size(), 0);
    });

    Reactor_.Run();
}

TEST_F(HttpHandleTest, RequestPerByteWriteTest) {
    Reactor_.StartCoroutine([this]() {
        std::string str = "POST /per_byte_test HTTP/1.1\r\nUser-Agent: portcullis-tester\r\n\r\n";
        for (size_t i = 0; i < str.size(); i++) {
            std::string s = str.substr(i, 1);
            FromClient_->WriteAll(s);
            Reactor()->Yield();
        }
        FromClient_->Close();
    });

    Reactor_.StartCoroutine([this]() {
        THttpHandle httpHandle(FromServer_);
        TResult<THttpRequest> res = httpHandle.ReadRequest();
        ASSERT_TRUE(res);
        const THttpRequest& req = res.Result();
        ASSERT_EQ(req.Url, "/per_byte_test");
        ASSERT_EQ(req.Method, "POST");
        ASSERT_EQ(req.MinorVersion, 1);
        ASSERT_EQ(req.Headers.size(), 1);
        ASSERT_EQ(req.Headers.at("User-Agent"), "portcullis-tester");
    });

    Reactor_.Run();
}

TEST_F(HttpHandleTest, MultipleRequestsTest) {
    Reactor_.StartCoroutine([this]() {
        for (size_t i = 0; i < 100; i++) {
            std::stringstream ss;
            ss << "GET /test/" << i << " HTTP/1.1\r\nUser-Agent: portcullis-tester\r\n\r\n";
            std::string str = ss.str();
            FromClient_->WriteAll(str);
            if (i % 10 == 0) {
                Reactor()->Yield();
            }
        }
        FromClient_->Close();
    });

    Reactor_.StartCoroutine([this]() {
        THttpHandle httpHandle(FromServer_);
        for (size_t i = 0; i < 100; i++) {
            TResult<THttpRequest> res = httpHandle.ReadRequest();
            ASSERT_TRUE(res);
            const THttpRequest& req = res.Result();
            std::stringstream ss;
            ss << "/test/" << i;
            ASSERT_EQ(req.Url, ss.str());
            ASSERT_EQ(req.Method, "GET");
            ASSERT_EQ(req.MinorVersion, 1);
            ASSERT_EQ(req.Headers.size(), 1);
            ASSERT_EQ(req.Headers.at("User-Agent"), "portcullis-tester");
        }
    });

    Reactor_.Run();
}

TEST_F(HttpHandleTest, MultipleIncompleteRequestsTest) {
    Reactor_.StartCoroutine([this]() {
        for (size_t i = 0; i < 100; i++) {
            std::string str = "GET / HTTP/1.1\r\nUser-Agent: portcullis-tester\r\n";
            FromClient_->WriteAll(str);
            Reactor()->Yield();
        }
        FromClient_->Close();
    });

    Reactor_.StartCoroutine([this]() {
        THttpHandle httpHandle(FromServer_);
        TResult<THttpRequest> res = httpHandle.ReadRequest();
        ASSERT_FALSE(res);
    });

    Reactor_.Run();
}

TEST_F(HttpHandleTest, IncompleteRequestTest) {
    Reactor_.StartCoroutine([this]() {
        std::string str = "GET / HTTP/1.";
        FromClient_->WriteAll(str);
        Reactor()->Yield();
        FromClient_->Close();
    });

    Reactor_.StartCoroutine([this]() {
        THttpHandle httpHandle(FromServer_);
        TResult<THttpRequest> res = httpHandle.ReadRequest();
        ASSERT_FALSE(res);
    });

    Reactor_.Run();
}

TEST_F(HttpHandleTest, ReadRequestBodyTest) {
    std::string bodyStr = "0123456789";
    Reactor_.StartCoroutine([this, bodyStr]() {
        std::string str = "POST /test HTTP/1.1\r\nContent-Length: 10\r\n\r\n";
        str += bodyStr;
        FromClient_->WriteAll(TMemoryRegion(str.data(), str.size()));
        Reactor()->Yield();
        FromClient_->Close();
    });

    Reactor_.StartCoroutine([this, &bodyStr]() {
        THttpHandle httpHandle(FromServer_);
        TResult<THttpRequest> res = httpHandle.ReadRequest();
        ASSERT_TRUE(res);
        const THttpRequest& req = res.Result();
        ASSERT_EQ(req.Url, "/test");
        ASSERT_EQ(req.Method, "POST");
        ASSERT_EQ(req.MinorVersion, 1);
        ASSERT_EQ(req.Headers.size(), 1);
        ASSERT_EQ(req.Headers.at("Content-Length"), "10");
        TResult<TMemoryRegion> ret = httpHandle.ReadBody(req);
        ASSERT_TRUE(ret);
        const TMemoryRegion& body = ret.Result();
        ASSERT_EQ(body.Size(), bodyStr.size());
        ASSERT_TRUE(body.EqualTo(bodyStr));
    });

    Reactor_.Run();
}

TEST_F(HttpHandleTest, TransferRequestBodyTest) {
    std::string bodyStr = "0123456789";
    Reactor_.StartCoroutine([this, bodyStr]() {
        std::string str = "POST /very_very_very_very_large_path HTTP/1.1\r\nContent-Length: 1000\r\n\r\n1234567890";
        FromClient_->WriteAll(str);
        str = "1234567890";
        for (size_t i = 0; i < 99; i++) {
            Reactor()->Yield();
            FromClient_->WriteAll(str);
        }
        FromClient_->Close();
    });

    Reactor_.StartCoroutine([this, &bodyStr]() {
        TTcpHandlePtr toBackend, fromBackend;
        TCoroutine* coro = Reactor_.StartAwaitableCoroutine([this, &toBackend, &fromBackend]() {
            CreateConnectedPair(toBackend, fromBackend);
        });
        Reactor()->Await(coro);

        THttpHandle httpHandle(FromServer_, 75);
        TResult<THttpRequest> res = httpHandle.ReadRequest();
        ASSERT_TRUE(res);
        const THttpRequest& req = res.Result();
        ASSERT_EQ(req.Url, "/very_very_very_very_large_path");
        THttpHandle handleToBackend(toBackend);
        httpHandle.TransferBody(handleToBackend, req);
        {
            TSocketBuffer buffer(1000);

            while (buffer.Size() < 1000) {
                TResult<size_t> res = fromBackend->Read(buffer);
                ASSERT_TRUE(res);
            }

            ASSERT_EQ(buffer.Size(), 1000);
            std::string body;
            body.reserve(1000);
            for (size_t i = 0; i < 100; i++) {
                body += "0123456789";
            }
            buffer.CurrentMemoryRegion().EqualTo(body);
        }

    });

    Reactor_.Run();
}

TEST_F(HttpHandleTest, ReadIncompleteRequestBodyTest) {
    std::string bodyStr = "0123456789";
    Reactor_.StartCoroutine([this, bodyStr]() {
        std::string str = "POST /test HTTP/1.1\r\nContent-Length: 1000\r\n\r\n";
        FromClient_->WriteAll(str);
        Reactor()->Yield();
        str = "012312";
        FromClient_->WriteAll(str);
        Reactor()->Yield();
        FromClient_->Close();
    });

    Reactor_.StartCoroutine([this, &bodyStr]() {
        THttpHandle httpHandle(FromServer_);
        TResult<THttpRequest> res = httpHandle.ReadRequest();
        ASSERT_TRUE(res);
        const THttpRequest& req = res.Result();
        ASSERT_EQ(req.Url, "/test");
        ASSERT_EQ(req.Method, "POST");
        ASSERT_EQ(req.MinorVersion, 1);
        ASSERT_EQ(req.Headers.size(), 1);
        TResult<TMemoryRegion> ret = httpHandle.ReadBody(req);
        ASSERT_FALSE(ret);
    });

    Reactor_.Run();
}

TEST_F(HttpHandleTest, WriteGetSimpleRequestTest) {
    Reactor_.StartCoroutine([this]() {
        THttpHandle httpHandle(FromClient_);
        THttpRequest request;
        request.Url = "/test_request_write?test=1&a=b";
        request.Method = "GET";
        request.MinorVersion = 1;
        request.Headers["User-Agent"] = "portcullis-tester";
        request.Headers["Host"] = "www.portcullis.com";
        httpHandle.WriteRequest(request);
        Reactor()->Yield();
        FromClient_->Close();
    });

    Reactor_.StartCoroutine([this]() {
        THttpHandle httpHandle(FromServer_);
        TResult<THttpRequest> res = httpHandle.ReadRequest();
        ASSERT_TRUE(res);
        const THttpRequest& req = res.Result();
        ASSERT_EQ(req.Url, "/test_request_write?test=1&a=b");
        ASSERT_EQ(req.Method, "GET");
        ASSERT_EQ(req.MinorVersion, 1);
        ASSERT_EQ(req.Headers.size(), 2);
        ASSERT_EQ(req.Headers.at("Host"), "www.portcullis.com");
        ASSERT_EQ(req.Headers.at("User-Agent"), "portcullis-tester");
    });

    Reactor_.Run();
}

TEST_F(HttpHandleTest, SimpleOkResponseTest) {
    Reactor_.StartCoroutine([this]() {
        std::string str = "HTTP/1.1 200 Ok\r\nServer: portcullis\r\n\r\n";
        FromClient_->WriteAll(str);
    });

    Reactor_.StartCoroutine([this]() {
        THttpHandle httpHandle(FromServer_);
        TResult<THttpResponse> res = httpHandle.ReadResponse();
        ASSERT_TRUE(res);
        const THttpResponse& req = res.Result();
        ASSERT_EQ(req.Status, 200);
        ASSERT_EQ(req.Reason, "Ok");
        ASSERT_EQ(req.MinorVersion, 1);
        ASSERT_EQ(req.Headers.size(), 1);
        ASSERT_EQ(req.Headers.at("Server"), "portcullis");
    });

    Reactor_.Run();
}

TEST_F(HttpHandleTest, ResponsePerByteWriteTest) {
    Reactor_.StartCoroutine([this]() {
        std::string str = "HTTP/1.1 200 Per Byte Test Is Ok\r\nServer: portcullis\r\n\r\n";
        for (size_t i = 0; i < str.size(); i++) {
            std::string s = str.substr(i, 1);
            FromClient_->WriteAll(s);
            Reactor()->Yield();
        }
        FromClient_->Close();
    });

    Reactor_.StartCoroutine([this]() {
        THttpHandle httpHandle(FromServer_);
        TResult<THttpResponse> res = httpHandle.ReadResponse();
        ASSERT_TRUE(res);
        const THttpResponse& resp = res.Result();
        ASSERT_EQ(resp.Status, 200);
        ASSERT_EQ(resp.Reason, "Per Byte Test Is Ok");
        ASSERT_EQ(resp.MinorVersion, 1);
        ASSERT_EQ(resp.Headers.size(), 1);
        ASSERT_EQ(resp.Headers.at("Server"), "portcullis");
    });

    Reactor_.Run();
}

TEST_F(HttpHandleTest, MultipleResponseTest) {
    Reactor_.StartCoroutine([this]() {
        for (size_t i = 0; i < 100; i++) {
            std::stringstream ss;
            ss << "HTTP/1.1 " << 200 + i << " Okaay\r\nServer: portcullis\r\n\r\n";
            std::string str = ss.str();
            FromClient_->WriteAll(str);
            if (i % 10 == 0) {
                Reactor()->Yield();
            }
        }
        FromClient_->Close();
    });

    Reactor_.StartCoroutine([this]() {
        THttpHandle httpHandle(FromServer_);
        for (size_t i = 0; i < 100; i++) {
            TResult<THttpResponse> res = httpHandle.ReadResponse();
            ASSERT_TRUE(res);
            const THttpResponse& resp = res.Result();
            ASSERT_EQ(resp.Status, 200 + i);
            ASSERT_EQ(resp.Reason, "Okaay");
            ASSERT_EQ(resp.MinorVersion, 1);
            ASSERT_EQ(resp.Headers.size(), 1);
            ASSERT_EQ(resp.Headers.at("Server"), "portcullis");
        }
    });

    Reactor_.Run();
}

TEST_F(HttpHandleTest, MultipleIncompleteResponseTest) {
    Reactor_.StartCoroutine([this]() {
        for (size_t i = 0; i < 100; i++) {
            std::string str = "HTTP/1.1 500 Internal Server Error\r\nUser-Agent: portcullis-tester\r\n";
            FromClient_->WriteAll(str);
            Reactor()->Yield();
        }
        FromClient_->Close();
    });

    Reactor_.StartCoroutine([this]() {
        THttpHandle httpHandle(FromServer_);
        TResult<THttpResponse> res = httpHandle.ReadResponse();
        ASSERT_FALSE(res);
    });

    Reactor_.Run();
}

TEST_F(HttpHandleTest, IncompleteResponseTest) {
    Reactor_.StartCoroutine([this]() {
        std::string str = "HTTP/1.1 400";
        FromClient_->WriteAll(str);
        Reactor()->Yield();
        FromClient_->Close();
    });

    Reactor_.StartCoroutine([this]() {
        THttpHandle httpHandle(FromServer_);
        TResult<THttpRequest> res = httpHandle.ReadRequest();
        ASSERT_FALSE(res);
    });

    Reactor_.Run();
}

TEST_F(HttpHandleTest, ReadResponseBodyTest) {
    std::string bodyStr = "0123456789";
    Reactor_.StartCoroutine([this, bodyStr]() {
        std::string str = "HTTP/1.1 200 Ok\r\nContent-Length: 10\r\n\r\n";
        str += bodyStr;
        FromClient_->WriteAll(str);
        Reactor()->Yield();
        FromClient_->Close();
    });

    Reactor_.StartCoroutine([this, &bodyStr]() {
        THttpHandle httpHandle(FromServer_);
        TResult<THttpResponse> res = httpHandle.ReadResponse();
        ASSERT_TRUE(res);
        const THttpResponse& resp = res.Result();
        ASSERT_EQ(resp.Status, 200);
        ASSERT_EQ(resp.Reason, "Ok");
        ASSERT_EQ(resp.MinorVersion, 1);
        ASSERT_EQ(resp.Headers.size(), 1);
        ASSERT_EQ(resp.Headers.at("Content-Length"), "10");
        TResult<TMemoryRegion> ret = httpHandle.ReadBody(resp);
        ASSERT_TRUE(ret);
        const TMemoryRegion& body = ret.Result();
        ASSERT_EQ(body.Size(), bodyStr.size());
        ASSERT_TRUE(body.EqualTo(bodyStr));
    });

    Reactor_.Run();
}

TEST_F(HttpHandleTest, TransferResponseBodyTest) {
    std::string bodyStr = "0123456789";
    Reactor_.StartCoroutine([this, bodyStr]() {
        std::string str = "HTTP/1.1 200 Ok\r\nContent-Length: 1000\r\n\r\n1234567890";
        FromClient_->WriteAll(str);
        str = "1234567890";
        for (size_t i = 0; i < 99; i++) {
            Reactor()->Yield();
            FromClient_->WriteAll(str);
        }
        FromClient_->Close();
    });

    Reactor_.StartCoroutine([this, &bodyStr]() {
        TTcpHandlePtr toBackend, fromBackend;
        TCoroutine* coro = Reactor_.StartAwaitableCoroutine([this, &toBackend, &fromBackend]() {
            CreateConnectedPair(toBackend, fromBackend);
        });
        Reactor()->Await(coro);

        THttpHandle httpHandle(FromServer_, 75);
        TResult<THttpResponse> res = httpHandle.ReadResponse();
        ASSERT_TRUE(res);
        const THttpResponse& resp = res.Result();
        THttpHandle handleToBackend(toBackend);
        httpHandle.TransferBody(handleToBackend, resp);
        {
            TSocketBuffer buffer(1000);

            while (buffer.Size() < 1000) {
                TResult<size_t> res = fromBackend->Read(buffer);
                ASSERT_TRUE(res);
            }

            ASSERT_EQ(buffer.Size(), 1000);
            std::string body;
            body.reserve(1000);
            for (size_t i = 0; i < 100; i++) {
                body += "0123456789";
            }
            buffer.CurrentMemoryRegion().EqualTo(body);
        }

    });

    Reactor_.Run();
}

TEST_F(HttpHandleTest, ReadIncompleteResponseBodyTest) {
    std::string bodyStr = "0123456789";
    Reactor_.StartCoroutine([this, bodyStr]() {
        std::string str = "HTTP/1.1 200 Ok\r\nContent-Length: 1000\r\n\r\n";
        FromClient_->WriteAll(str);
        Reactor()->Yield();
        str = "012312";
        FromClient_->WriteAll(str);
        Reactor()->Yield();
        FromClient_->Close();
    });

    Reactor_.StartCoroutine([this, &bodyStr]() {
        THttpHandle httpHandle(FromServer_);
        TResult<THttpResponse> res = httpHandle.ReadResponse();
        ASSERT_TRUE(res);
        const THttpResponse& resp = res.Result();
        TResult<TMemoryRegion> ret = httpHandle.ReadBody(resp);
        ASSERT_FALSE(ret);
    });

    Reactor_.Run();
}

TEST_F(HttpHandleTest, WriteSimpleOkResponseTest) {
    Reactor_.StartCoroutine([this]() {
        THttpHandle httpHandle(FromClient_);
        THttpResponse resp;
        resp.Status = 200;
        resp.Reason = "Ok";
        resp.MinorVersion = 1;
        resp.Headers["Server"] = "portcullis";
        httpHandle.WriteResponse(resp);
        Reactor()->Yield();
        FromClient_->Close();
    });

    Reactor_.StartCoroutine([this]() {
        THttpHandle httpHandle(FromServer_);
        TResult<THttpResponse> res = httpHandle.ReadResponse();
        ASSERT_TRUE(res);
        const THttpResponse& req = res.Result();
        ASSERT_EQ(req.Status, 200);
        ASSERT_EQ(req.Reason, "Ok");
        ASSERT_EQ(req.MinorVersion, 1);
        ASSERT_EQ(req.Headers.size(), 1);
        ASSERT_EQ(req.Headers.at("Server"), "portcullis");
    });

    Reactor_.Run();
}

int main(int argc, char* argv[]) {
    auto logger = spdlog::stdout_color_mt("reactor");
    logger->set_level(spdlog::level::debug);
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
