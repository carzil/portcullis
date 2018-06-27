#include <iostream>

#include <spdlog/spdlog.h>

#include "core/service.h"
#include "version.h"
#include "util/exception.h"

#include <pybind11/embed.h>

namespace py = pybind11;
using namespace std::placeholders;


int main(int argc, char** argv) {
    py::scoped_interpreter guard;
    TEventLoop loop;

    auto logger = spdlog::stdout_color_mt("main");

    if (argc < 2) {
        logger->critical("usage: portcullis CONFIG_FILE");
        return 1;
    }

    logger->info("running Portcullis (v{}, git@{})", PORTCULLIS_VERSION, PORTCULLIS_GIT_COMMIT);

    TService handler(&loop, argv[1]);

    try {
        handler.Start();
    } catch (const std::exception& e) {
        logger->critical("cannot start: {}", e.what());
        return 1;
    }

    loop.RunForever();

    return 0;
}
