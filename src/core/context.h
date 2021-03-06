#pragma once

#include <memory>
#include <spdlog/spdlog.h>
#include <pybind11/pybind11.h>

#include <unordered_map>

#include "fwd.h"
#include <handles/tcp.h>

namespace py = pybind11;

struct TConfig {
    std::string HandlerFile;

    std::string Host;
    std::string Port;
    size_t Backlog = 0;

    std::string Protocol;

    std::string BackendIp;
    std::string BackendIpv6;
    std::string BackendPort;

    size_t CoroutineStackSize = 0;
};

class TContext {
public:
    std::shared_ptr<spdlog::logger> Logger;
    TConfig Config;
    py::object HandlerObject;
    py::object HandlerModule;
    TSocketAddress BackendAddr;

    ~TContext() {
        Logger->info("context destroyed");
    }

    TSocketBufferPtr AllocBuffer(size_t size);
};

TConfig ReadConfigFromFile(std::string filename);
