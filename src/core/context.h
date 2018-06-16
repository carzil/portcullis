#pragma once

#include <memory>
#include <spdlog/spdlog.h>
#include <pybind11/pybind11.h>

#include <unordered_map>

#include "core/fwd.h"
#include "core/handle.h"
#include "shield/fwd.h"

namespace py = pybind11;

struct TConfig {
    std::string Name;
    std::string HandlerFile;

    std::string Host;
    std::string Port;

    size_t Backlog = 0;
};

class TSplicer;

class TContext : public std::enable_shared_from_this<TContext> {
public:
    std::shared_ptr<spdlog::logger> Logger;
    TConfig Config;
    TSocketHandlePtr Listener;
    py::object HandlerClass;
    py::object HandlerModule;
    TEventLoop* Loop = nullptr;

    TSocketAddress Resolve(const std::string& addr);

    ~TContext();

private:
    std::unordered_map<std::string, TSocketAddress> CachedAddrs_;
};
