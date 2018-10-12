#include <iostream>

#include "version.h"

#include <spdlog/spdlog.h>
#include <pybind11/embed.h>

#include <core/service.h>
#include <coro/coro.h>
#include <coro/reactor.h>
#include <handles/tcp.h>
#include <regexp/matchers.h>

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

    TStreamRegexpDatabase db({
        { "abc[def]{3}", HS_FLAG_CASELESS },
        { "abcdef", 0 }
    });

    TStreamRegexpMatcher matcher(db, [](unsigned int id, size_t from, size_t to) {
        std::cerr << id << " (" << from << ", " << to << ")" << std::endl;
        return false;
    });

    ::sigset_t sigs;
    ::sigemptyset(&sigs);
    ::sigaddset(&sigs, SIGPIPE);
    ::sigprocmask(SIG_BLOCK, &sigs, NULL);

    serviceLogger->info("running Portcullis (v{}, git@{}, {})", PORTCULLIS_VERSION, PORTCULLIS_GIT_COMMIT, PORTCULLIS_BUILD_TYPE);

    TReactor reactor(reactorLogger);

    std::string configPath = argv[1];

    TService service(serviceLogger, configPath);

    reactor.StartCoroutine([&service]() {
        service.Start();
    });

    reactor.Run();

    return 0;
}
