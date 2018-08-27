#include <unistd.h>

#include <coro/reactor.h>

#include <gtest/gtest.h>

class ReactorIoTest : public ::testing::Test {
protected:
    ReactorIoTest()
        : Reactor_(spdlog::get("reactor"))
    {}

    void SetUp() override {
        EXPECT_EQ(pipe2(Pipe1_, O_NONBLOCK), 0);
        EXPECT_EQ(pipe2(Pipe2_, O_NONBLOCK), 0);
        Reactor_.RegisterNonBlockingFd(Pipe1_[0]);
        Reactor_.RegisterNonBlockingFd(Pipe1_[1]);
        Reactor_.RegisterNonBlockingFd(Pipe2_[0]);
        Reactor_.RegisterNonBlockingFd(Pipe2_[1]);
    }

    void TearDown() override {
        Reactor_.CloseFd(Pipe1_[0]);
        Reactor_.CloseFd(Pipe1_[1]);

        Reactor_.CloseFd(Pipe2_[0]);
        Reactor_.CloseFd(Pipe2_[1]);

        close(Pipe1_[0]);
        close(Pipe1_[1]);

        close(Pipe2_[0]);
        close(Pipe2_[1]);
    }

    TReactor Reactor_;
    int Pipe1_[2];
    int Pipe2_[2];
};

TEST_F(ReactorIoTest, TestRead) {
    const char testStr[] = "hello, portcullis";

    Reactor_.StartCoroutine([this, testStr]() {
        char buf[30];
        TResult<size_t> res = Reactor()->Read(Pipe1_[0], buf, 30);
        EXPECT_TRUE(res);
        EXPECT_EQ(res.Result(), sizeof(testStr));

        for (size_t i = 0; i < res.Result(); i++) {
            EXPECT_EQ(buf[i], testStr[i]);
        }
    });

    Reactor_.StartCoroutine([this, testStr]() {
        write(Pipe1_[1], testStr, sizeof(testStr));
    });

    Reactor_.Run();
}

TEST_F(ReactorIoTest, TestWrite) {
    const char testStr[] = "hello, portcullis";

    Reactor_.StartCoroutine([this, testStr]() {
        TResult<size_t> res = Reactor()->Write(Pipe1_[1], testStr, sizeof(testStr));
        EXPECT_TRUE(res);
        EXPECT_EQ(res.Result(), sizeof(testStr));
    });

    Reactor_.Run();

    char buf[30];
    EXPECT_EQ(read(Pipe1_[0], buf, sizeof(testStr)), sizeof(testStr));

    for (size_t i = 0; i < sizeof(testStr); i++) {
        EXPECT_EQ(buf[i], testStr[i]);
    }
}

TEST_F(ReactorIoTest, TestEcho) {
    TCoroutine* echoCoroutine = Reactor_.StartCoroutine([this]() {
        while (true) {
            int count = 0;
            TResult<size_t> res = Reactor()->Read(Pipe1_[0], &count, sizeof(count));
            if (!res && res.Canceled()) {
                break;
            }
            EXPECT_TRUE(res);
            EXPECT_EQ(res.Result(), sizeof(count));

            Reactor()->Yield();

            count++;
            res = Reactor()->Write(Pipe2_[1], &count, sizeof(count));
            EXPECT_TRUE(res);
            EXPECT_EQ(res.Result(), sizeof(count));

            Reactor()->Yield();
        }
    });

    Reactor_.StartCoroutine([this, echoCoroutine]() {
        for (int count = 0; count < 2; count++) {
            TResult<size_t> res = Reactor()->Write(Pipe1_[1], &count, sizeof(count));
            EXPECT_TRUE(res);
            EXPECT_EQ(res.Result(), sizeof(count));

            Reactor()->Yield();

            int ans = 0;
            res = Reactor()->Read(Pipe2_[0], &ans, sizeof(ans));
            EXPECT_TRUE(res);
            EXPECT_EQ(res.Result(), sizeof(ans));
            EXPECT_EQ(count + 1, ans);

            Reactor()->Yield();
        }

        echoCoroutine->Cancel();
    });

    Reactor_.Run();
}

int main(int argc, char* argv[]) {
    auto logger = spdlog::stdout_color_mt("reactor");
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
