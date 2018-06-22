#include <pybind11/pytypes.h>

#include "core/context.h"
#include "python/wrappers.h"
#include "shield/splicer.h"

namespace py = pybind11;


TSplicer::TSplicer(TContextWrapper context, TSocketHandleWrapper client, TSocketHandleWrapper backend, py::object onEndCb)
    : Context_(context.Get())
    , Client_(client.Handle())
    , Backend_(backend.Handle())
    , ClientBuffer_(new TSocketBuffer(4096))
    , BackendBuffer_(new TSocketBuffer(4096))
    , Id_(Client_->Fd())
    , OnEndCb_(onEndCb)
{
}

TSplicer::~TSplicer() {
    End();
}

void TSplicer::Start() {
    if (Started_) {
        throw TException() << "splicer already started";
    }

    Started_ = true;

    Client_->Read(ClientBuffer_.get(), [this](TSocketHandlePtr, size_t bytesRead, bool eof) {
        if (eof) {
            ClientEof_ = true;
            if (BackendEof_) {
                End();
            } else {
                Client_->StopRead();
                Backend_->ShutdownWrite();
            }
        } else {
            FlushClientBuffer();
        }
    });

    Backend_->Read(BackendBuffer_.get(), [this](TSocketHandlePtr, size_t bytesRead, bool eof) {
        if (eof) {
            BackendEof_ = true;
            if (ClientEof_) {
                End();
            } else {
                Backend_->StopRead();
                Client_->ShutdownWrite();
            }
        } else {
            FlushBackendBuffer();
        }
    });
}

void TSplicer::FlushClientBuffer() {
    Client_->PauseRead();

    Backend_->Write(ClientBuffer_->CurrentMemoryRegion(), [this](TSocketHandlePtr) {
        ClientBuffer_->Reset();
        Client_->RestartRead();
        Backend_->RestartRead();
    });
}

void TSplicer::FlushBackendBuffer() {
    Backend_->PauseRead();

    Client_->Write(BackendBuffer_->CurrentMemoryRegion(), [this](TSocketHandlePtr) {
        BackendBuffer_->Reset();
        Backend_->RestartRead();
        Client_->RestartRead();
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
