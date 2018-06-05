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

    auto logger = spdlog::stdout_color_mt("portcullis");

    logger->info("running portcullis v" PORTCULLIS_VERSION ", git@" PORTCULLIS_GIT_COMMIT);

    py::object pyConfig = py::dict();
    py::eval_file(argv[1], pyConfig);

    TServiceConfig config;
    config.Name = pyConfig["name"].cast<std::string>();
    config.Host = pyConfig["host"].cast<std::string>();
    config.Port = pyConfig["port"].cast<std::string>();
    config.HandlerFile = pyConfig["handler_file"].cast<std::string>();

    config.BackendHost = pyConfig["backend_host"].cast<std::string>();
    config.BackendPort = pyConfig["backend_port"].cast<std::string>();

    TServiceContext context;
    context.Config = config;
    context.Logger = logger;

    TService handler(&loop, context);
    handler.Start();

    loop.RunForever();

    return 0;
}
