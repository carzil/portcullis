#include <iostream>

#include <spdlog/spdlog.h>

#include "exception.h"
#include "service.h"
#include "resource.h"
#include "version.h"

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
    handler.Start();

    loop.RunForever();

    return 0;
}
