#include <pybind11/pytypes.h>

#include "core/context.h"
#include "python/wrappers.h"
#include "shield/splicer.h"

namespace py = pybind11;


TSplicer::TSplicer(TContextWrapper context, TSocketHandleWrapper client, TSocketHandleWrapper backend)
    : Context_(context.Get())
    , Client_(client.Handle())
    , Backend_(backend.Handle())
    , ClientBuffer_(new TSocketBuffer(4096))
    , BackendBuffer_(new TSocketBuffer(4096))
    , Id_(Client_->Fd())
{
}

TSplicer::~TSplicer() {
    End();
}

void TSplicer::Start(py::object onEndCb) {
    if (Started_) {
        throw TException() << "splicer already started";
    }

    OnEndCb_ = onEndCb;
    Started_ = true;

    Client_->Read(ClientBuffer_.get(), [this](TSocketHandlePtr, size_t bytesRead, bool eof) {
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

    Backend_->Read(BackendBuffer_.get(), [this](TSocketHandlePtr, size_t bytesRead, bool eof) {
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
    Client_->ShutdownAll();
    Backend_->ShutdownAll();
    Client_->Close();
    Backend_->Close();

    if (!OnEndCb_.is_none()) {
        OnEndCb_();
        OnEndCb_ = py::none();
    }
}
