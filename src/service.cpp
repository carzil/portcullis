#include <csignal>
#include <deque>
#include <exception>
#include <functional>
#include <systemd/sd-daemon.h>

#include "service.h"
#include "version.h"

using namespace std::placeholders;

static py::object PyEvalFile(const std::string& path) {
    py::object module = py::dict();
    module["__builtins__"] = PyEval_GetBuiltins();
    py::eval_file(path, module);
    return module;
}


TConnection::TConnection(TService* parent, TServiceContextPtr context, TSocketHandlePtr client, TSocketHandlePtr backend)
    : Parent_(parent)
    , Context_(context)
    , Client_(std::move(client))
    , Backend_(std::move(backend))
    , ClientBuffer_(4096)
    , BackendBuffer_(4096)
    , Handler_(context->HandlerClass())
    , Id_(Client_->Fd())
{
    ASSERT(Id_ != -1);
    Client_->Read(&ClientBuffer_, std::bind(&TConnection::ReadFromClient, this, _1, _2, _3));
    Backend_->Read(&BackendBuffer_, std::bind(&TConnection::ReadFromBackend, this, _1, _2, _3));
}

void TConnection::ReadFromClient(TSocketHandlePtr client, size_t readBytes, bool eof) {
    if (eof) {
        End();
        return;
    }

    client->PauseRead();

    Backend_->Write(ClientBuffer_.CurrentMemoryRegion(), [this](TSocketHandlePtr backend) {
        ClientBuffer_.Reset();
        Client_->RestartRead();
        Backend_->RestartRead();
    });
}

void TConnection::ReadFromBackend(TSocketHandlePtr backend, size_t readBytes, bool eof) {
    if (eof) {
        End();
        return;
    }

    backend->PauseRead();

    Client_->Write(BackendBuffer_.CurrentMemoryRegion(), [this](TSocketHandlePtr client) {
        BackendBuffer_.Reset();
        Backend_->RestartRead();
        Client_->RestartRead();
    });
}

void TConnection::End() {
    Backend_->Close();
    Client_->Close();
    Parent_->FinishConnection(this);
}


TService::TService(TEventLoop* loop, const std::string& configPath)
    : Loop_(loop)
    , ConfigPath_(configPath)
{
    std::shared_ptr<TServiceContext> context = ReloadContext();
    atomic_store(&Context_, context);
}

std::shared_ptr<TServiceContext> TService::ReloadContext() {
    std::shared_ptr<TServiceContext> oldContext = std::atomic_load(&Context_);

    auto logger = spdlog::get("main");

    logger->info("loading config from file {}", AbsPath(ConfigPath_));

    py::object pyConfig = PyEvalFile(ConfigPath_);

    TServiceConfig config;
    config.Name = pyConfig["name"].cast<std::string>();
    config.Host = pyConfig["host"].cast<std::string>();
    config.Port = pyConfig["port"].cast<std::string>();
    config.Backlog = pyConfig["backlog"].cast<size_t>();
    config.HandlerFile = pyConfig["handler_file"].cast<std::string>();

    config.BackendHost = pyConfig["backend_host"].cast<std::string>();
    config.BackendPort = pyConfig["backend_port"].cast<std::string>();

    py::object handlerModule = PyEvalFile(config.HandlerFile);

    TServiceContext context;
    context.Config = config;
    context.HandlerClass = handlerModule["Handler"];
    context.HandlerModule = std::move(handlerModule);
    context.Logger = logger;

    std::vector<TSocketAddress> addrs = GetAddrInfo(context.Config.Host, context.Config.Port, true, "tcp");

    if (!oldContext || (oldContext->Listener->Address() != addrs[0])) {
        context.Listener = Loop_->MakeTcp();
        context.Listener->ReuseAddr();
        context.Listener->Bind(addrs[0]);
        context.Listener->Listen(std::bind(&TService::StartConnection, this, _1, _2), context.Config.Backlog);

        context.Logger->info("listening on {}:{}", addrs[0].Host(), addrs[0].Port());
    } else {
        context.Listener = oldContext->Listener;
    }

    addrs = GetAddrInfo(context.Config.BackendHost, context.Config.BackendPort, false, "tcp");
    context.BackendAddr = addrs[0];

    return std::make_shared<TServiceContext>(std::move(context));
}

void TService::StartConnection(TSocketHandlePtr listener, TSocketHandlePtr client) {
    std::shared_ptr<TServiceContext> context = std::atomic_load(&Context_);

    TSocketHandlePtr backend = Loop_->MakeTcp();
    backend->Connect(context->BackendAddr, [this, client, context](TSocketHandlePtr backend) {
        std::unique_ptr<TConnection> connection(new TConnection(this, std::move(context), std::move(client), std::move(backend)));
        if (Connections_.size() <= connection->Id()) {
            Connections_.resize(connection->Id() + 1);
        }
        Connections_[connection->Id()] = std::move(connection);
    });
}

void TService::Shutdown() {
    ::sd_notify(0, "STOPPING=1");
    Context_->Logger->info("shutdown request");
    Loop_->Shutdown();
}

void TService::Start() {
    if (!Context_) {
        return;
    }

    Loop_->Signal(SIGINT, [this](TSignalInfo info) {
        Context_->Logger->info("caught SIGINT from pid {}", info.Sender());
        Shutdown();
    });

    Loop_->Signal(SIGTERM, [this](TSignalInfo info) {
        Context_->Logger->info("caught SIGTERM from pid {}", info.Sender());
        Shutdown();
    });

    Loop_->Signal(SIGUSR1, [this](TSignalInfo info) {
        Context_->Logger->info("caught SIGUSR1 from {}", info.Sender());
        try {
            ::sd_notify(0, "RELOADING=1");
            std::shared_ptr<TServiceContext> newContext = ReloadContext();
            std::atomic_store(&Context_, newContext);
            ::sd_notify(0, "READY=1");
            newContext->Logger->info("context reloaded");
        } catch (const std::exception& e) {
            Context_->Logger->error("context reload failed: {}", e.what());
        }
    });

    Loop_->Cleanup([this]() {
        FinishedConnections_.clear();
    });

    // Well, loop not started yet, but we're almost ready
    ::sd_notify(0, "READY=1");
}
