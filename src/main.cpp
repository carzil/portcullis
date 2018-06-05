#include <iostream>

#include <spdlog/spdlog.h>

#include "exception.h"
#include "service.h"
#include "resource.h"

#include <pybind11/embed.h>

namespace py = pybind11;
using namespace std::placeholders;


int main(int argc, char** argv) {
    py::scoped_interpreter guard;
    TEventLoop loop;

    py::object pyConfig = py::dict();
    py::eval_file(argv[1], pyConfig);

    TServiceConfig config;
    config.Name = pyConfig["name"].cast<std::string>();
    config.Host = pyConfig["host"].cast<std::string>();
    config.Port = pyConfig["port"].cast<std::string>();
    config.HandlerFile = pyConfig["handler_file"].cast<std::string>();

    config.BackendHost = "localhost";
    config.BackendPort = "8000";

    TServiceContext context;
    context.Config = config;
    context.Logger = spdlog::stdout_color_mt("service");

    TService handler(&loop, context);
    handler.Start();

    loop.RunForever();

    return 0;
}
