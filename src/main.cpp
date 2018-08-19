#include <iostream>

#include "version.h"

#include <spdlog/spdlog.h>
#include <pybind11/embed.h>

#include <core/service.h>
#include <coro/coro.h>
#include <coro/reactor.h>
#include <coro/tcp_handle.h>

#include <signal.h>
#include <unistd.h>

namespace py = pybind11;
using namespace std::placeholders;

int main(int argc, char** argv) {
    py::scoped_interpreter guard;

    auto logger = spdlog::stdout_color_mt("main");
    auto reactorLogger = spdlog::stdout_color_mt("reactor");

#ifdef _DEBUG_
    logger->set_level(spdlog::level::debug);
    reactorLogger->set_level(spdlog::level::debug);
#endif

    if (argc < 2) {
        logger->critical("usage: portcullis CONFIG_FILE");
        return 1;
    }

    ::sigset_t sigs;
    ::sigemptyset(&sigs);
    ::sigaddset(&sigs, SIGPIPE);
    ::sigprocmask(SIG_BLOCK, &sigs, NULL);

    logger->info("running Portcullis (v{}, git@{}, {})", PORTCULLIS_VERSION, PORTCULLIS_GIT_COMMIT, PORTCULLIS_BUILD_TYPE);

    TReactor reactor(reactorLogger);

    std::string configPath = argv[1];

    TService service(configPath);

    reactor.StartCoroutine([&service]() {
        service.Start();
    });

    reactor.Run();

    return 0;
}
