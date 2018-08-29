#include <unistd.h>
#include <signal.h>

#include <coro/reactor.h>

#include <gtest/gtest.h>

TEST(ReactorCoreTest, CreateCoroutine) {
    TReactor reactor(spdlog::get("reactor"));
    bool started = false;
    reactor.StartCoroutine([&started]() {
        started = true;
    });
    reactor.Run();
    EXPECT_TRUE(started);
}

TEST(ReactorCoreTest, Yield) {
    TReactor reactor(spdlog::get("reactor"));
    reactor.StartCoroutine([]() {
        int counter = 0;
        for (int i = 0; i < 10; i++) {
            Reactor()->StartCoroutine([&counter]() {
                counter++;
            });
        }
        Reactor()->Yield();
        EXPECT_EQ(counter, 10);
    });
    reactor.Run();
}

TEST(ReactorCoreTest, Await) {
    TReactor reactor(spdlog::get("reactor"));

    bool awaitDone = false;

    TCoroutine* a = reactor.StartAwaitableCoroutine([&awaitDone]() {
        Reactor()->Yield();
        EXPECT_FALSE(awaitDone);

    });

    reactor.StartCoroutine([a, &awaitDone]() {
        Reactor()->Await(a);
        awaitDone = true;
    });

    reactor.Run();

    EXPECT_TRUE(awaitDone);
}

TEST(ReactorCoreTest, AwaitCancelsAwaitedCoroutine) {
    TReactor reactor(spdlog::get("reactor"));

    TCoroutine* a = reactor.StartAwaitableCoroutine([]() {
        while (!Reactor()->Current()->Canceled) {
            Reactor()->Yield();
        }
    });

    TCoroutine* b = reactor.StartAwaitableCoroutine([a]() {
        Reactor()->Await(a);
    });

    reactor.StartCoroutine([b]() {
        b->Cancel();
        b->Await();
    });

    reactor.Run();
}

TEST(ReactorCoreTest, AwaitAll) {
    TReactor reactor(spdlog::get("reactor"));

    bool awaitDone = false;

    TCoroutine* a = reactor.StartAwaitableCoroutine([&awaitDone]() {
        /* immediately exits  */
        EXPECT_FALSE(awaitDone);
    });

    TCoroutine* b = reactor.StartAwaitableCoroutine([&awaitDone]() {
        EXPECT_FALSE(awaitDone);
        Reactor()->Yield();
        EXPECT_FALSE(awaitDone);
    });

    TCoroutine* c = reactor.StartAwaitableCoroutine([&awaitDone]() {
        EXPECT_FALSE(awaitDone);
        Reactor()->Yield();
        EXPECT_FALSE(awaitDone);
        Reactor()->Yield();
        EXPECT_FALSE(awaitDone);
    });

    reactor.StartCoroutine([a, b, c, &awaitDone]() {
        TCoroutine* d = Reactor()->StartAwaitableCoroutine([&awaitDone]() {
            /* immediately exits  */
            EXPECT_FALSE(awaitDone);
        });

        Reactor()->AwaitAll({ a, b, c, d });
        awaitDone = true;
    });

    reactor.Run();

    EXPECT_TRUE(awaitDone);
}

TEST(ReactorCoreTest, AwaitAllCancelsAwaitedCoroutines) {
    TReactor reactor(spdlog::get("reactor"));

    TCoroutine* a = reactor.StartAwaitableCoroutine([]() {
        while (!Reactor()->Current()->Canceled) {
            Reactor()->Yield();
        }
    });

    TCoroutine* b = reactor.StartAwaitableCoroutine([]() {
        while (!Reactor()->Current()->Canceled) {
            Reactor()->Yield();
        }
    });

    TCoroutine* c = reactor.StartAwaitableCoroutine([a, b]() {
        Reactor()->AwaitAll({ a, b });
    });

    reactor.StartCoroutine([c]() {
        c->Cancel();
        c->Await();
    });

    reactor.Run();
}

TEST(ReactorCoreTest, SingleSignalHandlerCalled) {
    TReactor reactor(spdlog::get("reactor"));

    bool called = false;
    reactor.OnSignal(SIGUSR1, [&called](TSignalInfo) {
        called = true;
    });

    reactor.StartCoroutine([&called]() {
        while (!called) {
            Reactor()->Yield();
        }
    });

    reactor.StartCoroutine([]() {
        kill(getpid(), SIGUSR1);
    });

    reactor.Run();

    EXPECT_TRUE(called);
}

TEST(ReactorCoreTest, MultipleSignalHandlerCalled) {
    TReactor reactor(spdlog::get("reactor"));

    TSignalSet set;
    set.Add(SIGUSR1);
    set.Add(SIGUSR2);

    int called = 0;
    reactor.OnSignals(set, [&called](TSignalInfo) {
        called++;
    });

    reactor.StartCoroutine([&called]() {
        while (called < 2) {
            Reactor()->Yield();
        }
    });

    reactor.StartCoroutine([]() {
        kill(getpid(), SIGUSR1);
        kill(getpid(), SIGUSR2);
    });

    reactor.Run();

    EXPECT_TRUE(called == 2);
}

TEST(ReactorCoreTest, MultipleCancel) {
    TReactor reactor(spdlog::get("reactor"));

    TCoroutine* a = reactor.StartAwaitableCoroutine([]() {
        while (!Reactor()->Current()->Canceled) {
            Reactor()->Yield();
        }
    });

    reactor.StartCoroutine([a]() {
        for (int i = 0; i < 10; i++) {
            a->Cancel();
            Reactor()->Yield();
        }
        a->Await();
    });

    reactor.Run();
}

int main(int argc, char* argv[]) {
    auto logger = spdlog::stdout_color_mt("reactor");
    logger->set_level(spdlog::level::debug);
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
