#include <iostream>

#include "version.h"

#include <spdlog/spdlog.h>
#include <pybind11/embed.h>

#include <core/service.h>
#include <coro/coro.h>
#include <coro/reactor.h>
#include <handles/tcp.h>

#include <signal.h>
#include <unistd.h>

namespace py = pybind11;
using namespace std::placeholders;

int main(int argc, char** argv) {
    py::scoped_interpreter guard;

    auto serviceLogger = spdlog::stdout_color_mt("service");
    auto reactorLogger = spdlog::stdout_color_mt("reactor");

#ifdef _DEBUG_
    serviceLogger->set_level(spdlog::level::debug);
    reactorLogger->set_level(spdlog::level::debug);
#endif

    if (argc < 2) {
        serviceLogger->critical("usage: portcullis CONFIG_FILE");
        return 1;
    }

    ::sigset_t sigs;
    ::sigemptyset(&sigs);
    ::sigaddset(&sigs, SIGPIPE);
    ::sigprocmask(SIG_BLOCK, &sigs, NULL);

    serviceLogger->info("running Portcullis (v{}, git@{}, {})", PORTCULLIS_VERSION, PORTCULLIS_GIT_COMMIT, PORTCULLIS_BUILD_TYPE);

    TReactor reactor(reactorLogger, 4096 * 16);

    std::string configPath = argv[1];

    TService service(serviceLogger, configPath);

    reactor.StartCoroutine([&service]() {
        service.Start();
    });

    reactor.Run();

    return 0;
}
