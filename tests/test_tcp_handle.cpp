#include <coro/reactor.h>
#include <handles/tcp.h>

#include <gtest/gtest.h>

/* TODO: randomize port */

class TcpHandleTest : public ::testing::Test {
public:
    TcpHandleTest()
        : Reactor_(spdlog::get("reactor"))
    {}

    void SetUp() override {
        TTcpHandlePtr listener = TTcpHandle::Create();
        TSocketAddress bindAdr = GetAddrInfo("", "31337", true, "tcp")[0];
        listener.Bind(bindAddr);
        listener.Listen(1);
    }

    TReactor Reactor_;
    TTcpHandlePtr Server_;
    TTcpHandlePtr Client_;
};

TEST(HttpHandleTest, BasicTest) {
    TTcpHandlePtr server = TTcpHandle::Create();
}
