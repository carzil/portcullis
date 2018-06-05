#include <deque>
#include <exception>

#include <csignal>

#include "service.h"

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
    Listener_->Listen([this, context](std::shared_ptr<TSocketHandle> listener, std::shared_ptr<TSocketHandle> accepted) {
        context->Logger->info("connected client {}:{}", accepted->Address().Host(), accepted->Address().Port());

        std::shared_ptr<TSocketBuffer> buffer(new TSocketBuffer(4096));
        int* cnt = new int(0);
        accepted->Read([this, context, buffer, cnt](std::shared_ptr<TSocketHandle> client, size_t readBytes, bool eof) {
            context->Logger->info("read {} bytes, eof = {}", readBytes, eof);

            *cnt += readBytes;
            if (eof) {
                client->Close();
            } else if (*cnt > 25) {
                context->Logger->info("cnt = {}", *cnt);
                client->Write([context, buffer](TSocketHandlePtr client) {
                    context->Logger->info("write complete");
                    buffer->Unwind();
                }, buffer->CurrentMemoryRegion());
            }
        }, buffer.get());
    });
}

void TService::Start() {
}
