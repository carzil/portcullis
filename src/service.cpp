#include <csignal>
#include <deque>
#include <exception>
#include <functional>

#include "service.h"

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


TService::TService(TEventLoop* loop, const TServiceContext& serviceContext)
    : Loop_(loop)
    , Context_(std::make_shared<TServiceContext>(serviceContext))
{
    std::shared_ptr<TServiceContext> context = Context_;

    Loop_->Signal(SIGINT, [this, context](int sig) {
        context->Logger->info("got sigint");
        Loop_->Shutdown();
    });

    Listener_ = Loop_->MakeTcp();
    std::vector<TSocketAddress> addrs = GetAddrInfo(Context_->Config.Host, Context_->Config.Port, true, "tcp");
    Listener_->Bind(addrs[0]);

    addrs = GetAddrInfo(Context_->Config.BackendHost, Context_->Config.BackendPort, false, "tcp");
    TSocketAddress backendAddr = addrs[0];
    Listener_->Listen([this, context, backendAddr](TSocketHandlePtr listener, TSocketHandlePtr client) {
        // context->Logger->info("connected client {}:{}", client->Address().Host(), client->Address().Port());

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

void TService::Start() {
}
