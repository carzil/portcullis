#pragma once

#include <stdio.h>

#include <pybind11/eval.h>

#include <deque>

#include "core/loop.h"
#include "core/resource.h"
#include "core/utils.h"
#include "core/context.h"

namespace py = pybind11;

class TService {
public:
    TService(TEventLoop* loop, const std::string& configPath);
    void Start();
    void Shutdown();

    void StartConnection(TSocketHandlePtr listener, TSocketHandlePtr client);

private:
    std::shared_ptr<TContext> ReloadContext();

    std::shared_ptr<TContext> Context_;
    TEventLoop* Loop_ = nullptr;
    std::shared_ptr<TSocketHandle> Listener_;
    std::string ConfigPath_;
};
