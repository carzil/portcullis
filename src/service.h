#pragma once

#include <stdio.h>

#include <spdlog/spdlog.h>
#include <pybind11/eval.h>

#include "loop.h"
#include "resource.h"
#include "utils.h"


namespace py = pybind11;

struct TServiceConfig {
    std::string Name;
    std::string HandlerFile;

    std::string Host;
    std::string Port;

    std::string BackendHost;
    std::string BackendPort;
};

struct TServiceContext {
    std::shared_ptr<spdlog::logger> Logger;
    TServiceConfig Config;
};

class TService {
public:
    TService(TEventLoop* loop, const TServiceContext& context);
    void Start();

private:
    std::shared_ptr<TServiceContext> Context_;
    TEventLoop* Loop_ = nullptr;
    std::shared_ptr<TSocketHandle> Listener_;
};
