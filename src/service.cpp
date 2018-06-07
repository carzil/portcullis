#include <csignal>
#include <deque>
#include <exception>
#include <functional>

#include "service.h"
#include "version.h"

using namespace std::placeholders;

TConnection::TConnection(TService* parent, TServiceContextPtr context, TSocketHandlePtr client, TSocketHandlePtr backend)
    : Parent_(parent)
    , Context_(context)
    , Client_(std::move(client))
    , Backend_(std::move(backend))
    , ClientBuffer_(4096)
    , BackendBuffer_(4096)
{
    Client_->Read(&ClientBuffer_, std::bind(&TConnection::ReadFromClient, this, _1, _2, _3));
    Backend_->Read(&BackendBuffer_, std::bind(&TConnection::ReadFromBackend, this, _1, _2, _3));
}

void TConnection::ReadFromClient(TSocketHandlePtr client, size_t readBytes, bool eof) {
    if (eof) {
        Client_->Close();
        Backend_->Close();
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
        Backend_->Close();
        Client_->Close();
        return;
    }

    backend->PauseRead();

    Client_->Write(BackendBuffer_.CurrentMemoryRegion(), [this](TSocketHandlePtr client) {
        BackendBuffer_.Reset();
        Backend_->RestartRead();
        Client_->RestartRead();
    });
}


TService::TService(TEventLoop* loop, const std::string& configPath)
    : Loop_(loop)
    , ConfigPath_(configPath)
{
    std::shared_ptr<TServiceContext> context = ReloadContext();
    atomic_store(&Context_, context);
}

std::shared_ptr<TServiceContext> TService::ReloadContext() {
    py::object pyConfig = py::dict();
    py::eval_file(ConfigPath_, pyConfig);

    TServiceConfig config;
    config.Name = pyConfig["name"].cast<std::string>();
    config.Host = pyConfig["host"].cast<std::string>();
    config.Port = pyConfig["port"].cast<std::string>();
    config.Backlog = pyConfig["backlog"].cast<size_t>();
    config.HandlerFile = pyConfig["handler_file"].cast<std::string>();

    config.BackendHost = pyConfig["backend_host"].cast<std::string>();
    config.BackendPort = pyConfig["backend_port"].cast<std::string>();

    TServiceContext context;
    context.Config = config;
    context.Logger = spdlog::get("main");

    return std::make_shared<TServiceContext>(std::move(context));
}

void TService::Start() {
    Context_->Logger->info("Portcullis (v{}, git@{}) guarding service '{}'", PORTCULLIS_VERSION, PORTCULLIS_GIT_COMMIT, Context_->Config.Name);

    Loop_->Signal(SIGINT, [this](int sig) {
        Context_->Logger->info("got sigint");
        Loop_->Shutdown();
    });

    Loop_->Signal(SIGUSR1, [this](int sig) {
        Context_->Logger->info("reloading config");
        std::shared_ptr<TServiceContext> newContext = ReloadContext();
        std::atomic_store(&Context_, std::move(newContext));
        Context_->Logger->info("config reloaded");
    });

    Listener_ = Loop_->MakeTcp();
    std::vector<TSocketAddress> addrs = GetAddrInfo(Context_->Config.Host, Context_->Config.Port, true, "tcp");
    Listener_->Bind(addrs[0]);

    Context_->Logger->info("listening on {}:{}", addrs[0].Host(), addrs[0].Port());

    Listener_->Listen([this](TSocketHandlePtr listener, TSocketHandlePtr client) {
        std::shared_ptr<TServiceContext> context = std::atomic_load(&Context_);

        std::vector<TSocketAddress> addrs = GetAddrInfo(Context_->Config.BackendHost, Context_->Config.BackendPort, false, "tcp");
        TSocketAddress backendAddr = addrs[0];

        TSocketHandlePtr backend = Loop_->MakeTcp();
        backend->Connect(backendAddr, [this, client, context](TSocketHandlePtr backend) {
            std::unique_ptr<TConnection> connection(new TConnection(this, std::move(context), std::move(client), std::move(backend)));
            if (Connections_.size() <= connection->Id()) {
                Connections_.resize(connection->Id() + 1);
            }
            Connections_[connection->Id()] = std::move(connection);
        });
    }, Context_->Config.Backlog);
}
