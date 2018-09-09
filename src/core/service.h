#pragma once

#include <stdio.h>

#include <deque>

#include <core/context.h>
#include <coro/reactor.h>
#include <handles/tcp.h>

namespace py = pybind11;

class TService {
public:
    TService(std::shared_ptr<spdlog::logger> logger, const std::string& configPath);
    void Start();
    ~TService();

private:
    std::shared_ptr<TContext> ReloadContext();

    std::shared_ptr<spdlog::logger> Logger_;
    std::shared_ptr<TContext> Context_;
    TTcpListener Listener_;
    std::string ConfigPath_;
    PyThreadState* MainState_ = nullptr;
    std::list<PyThreadState*> PyStates_;
};
