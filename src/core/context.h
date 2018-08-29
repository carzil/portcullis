#pragma once

#include <memory>
#include <spdlog/spdlog.h>
#include <pybind11/pybind11.h>

#include <unordered_map>

#include "fwd.h"
#include <coro/tcp_handle.h>

namespace py = pybind11;

struct TConfig {
    std::string Name;
    std::string HandlerFile;

    std::string Host;
    std::string Port;
    size_t Backlog = 0;

    std::string Protocol;
    bool AllowIpv6 = false;
    bool Ipv6Only = false;

    std::string BackendHost;
    std::string BackendPort;
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
