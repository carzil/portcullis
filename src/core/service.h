#pragma once

#include <stdio.h>

#include <deque>

#include <core/context.h>
#include <coro/reactor.h>
#include <coro/tcp_handle.h>

namespace py = pybind11;

class TService {
public:
    TService(const std::string& configPath);
    void Start();

private:
    std::shared_ptr<TContext> ReloadContext();

    std::shared_ptr<TContext> Context_;
    TTcpHandlePtr Listener_;
    std::string ConfigPath_;
};
