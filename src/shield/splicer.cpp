#include "core/context.h"
#include "shield/splicer.h"


TSplicer::TSplicer(TContextPtr context, TSocketHandlePtr client, TSocketHandlePtr backend)
    : Context_(context)
    , Client_(std::move(client))
    , Backend_(std::move(backend))
    , ClientBuffer_(new TSocketBuffer(4096))
    , BackendBuffer_(new TSocketBuffer(4096))
    , Id_(Client_->Fd())
{
    Transfer();
}

TSplicer::~TSplicer() {
    Client_->Close();
    Backend_->Close();
}

void TSplicer::Transfer() {
    Client_->Read(ClientBuffer_.get(), [this](TSocketHandlePtr, size_t, bool eof) {
        if (eof) {
            End();
            return;
        }

        Client_->PauseRead();

        Backend_->Write(ClientBuffer_->CurrentMemoryRegion(), [this](TSocketHandlePtr) {
            ClientBuffer_->Reset();
            Client_->RestartRead();
            Backend_->RestartRead();
        });
    });

    Backend_->Read(BackendBuffer_.get(), [this](TSocketHandlePtr, size_t, bool eof) {
        if (eof) {
            End();
            return;
        }

        Backend_->PauseRead();

        Client_->Write(BackendBuffer_->CurrentMemoryRegion(), [this](TSocketHandlePtr) {
            BackendBuffer_->Reset();
            Backend_->RestartRead();
            Client_->RestartRead();
        });
    });
}

void TSplicer::End() {
    Context_->FinishSplicer(this);
}
