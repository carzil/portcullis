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

using TServiceContextPtr = std::shared_ptr<TServiceContext>;

class TService;

class TConnection {
public:
    TConnection(TService* parent, TServiceContextPtr context, TSocketHandlePtr client, TSocketHandlePtr backend);

    void ReadFromClient(TSocketHandlePtr client, size_t readBytes, bool eof);
    void ReadFromBackend(TSocketHandlePtr backend, size_t readBytes, bool eof);

    int Id() const {
        return Client_->Fd();
    }

private:
    TService* Parent_ = nullptr;
    TServiceContextPtr Context_;
    TSocketHandlePtr Client_;
    TSocketHandlePtr Backend_;
    TSocketBuffer ClientBuffer_;
    TSocketBuffer BackendBuffer_;
};

class TService {
public:
    TService(TEventLoop* loop, const TServiceContext& context);
    void Start();

    void EndConnection(TConnection* connection) {
        Connections_[connection->Id()] = nullptr;
    }

private:
    std::shared_ptr<TServiceContext> Context_;
    TEventLoop* Loop_ = nullptr;
    std::shared_ptr<TSocketHandle> Listener_;
    std::vector<std::unique_ptr<TConnection>> Connections_;
};
