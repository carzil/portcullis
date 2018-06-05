#include <deque>
#include <exception>

#include <csignal>

#include "service.h"

TService::TService(TEventLoop* loop, const TServiceContext& serviceContext)
    : Loop_(loop)
    , Context_(std::make_shared<TServiceContext>(serviceContext))
{
    std::shared_ptr<TServiceContext> context = Context_;

    // Loop_->Signal(SIGINT, [this, context](int sig) {
    //     context->Logger->info("got sigint");
    //     Loop_->Shutdown();
    // });

    Listener_ = Loop_->MakeTcp();
    std::vector<TSocketAddress> addrs = GetAddrInfo(Context_->Config.Host, Context_->Config.Port, true, "tcp");
    Listener_->Bind(addrs[0]);
    Listener_->Listen([this, context](std::shared_ptr<TSocketHandle> listener, std::shared_ptr<TSocketHandle> accepted) {
        context->Logger->info("connected client!");
    });
}

void TService::Start() {
}
