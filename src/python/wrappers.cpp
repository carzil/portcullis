#include "python/wrappers.h"

void TSocketHandleWrapper::Read(py::object handler) {
    std::shared_ptr<TSocketBuffer> buffer(new TSocketBuffer(1024));
    Handle_->Read(buffer.get(), [buffer, handler](TSocketHandlePtr handle, size_t readBytes, bool eof) {
        py::object view = py::memoryview(py::buffer_info(reinterpret_cast<uint8_t*>(buffer->Data()), readBytes));
        handler(TSocketHandleWrapper(std::move(handle)), view);
    });
}

void TSocketHandleWrapper::Write(std::string bytes, py::object handler) {
    std::shared_ptr<TSocketBuffer> buffer(new TSocketBuffer(bytes.size()));
    memcpy(buffer->Data(), bytes.data(), bytes.size());
    buffer->Advance(bytes.size());
    Handle_->Write(buffer->CurrentMemoryRegion(), [buffer, handler](TSocketHandlePtr handle) {
        handler(TSocketHandleWrapper(std::move(handle)));
    });
}

void TSocketHandleWrapper::Close() {
    Handle_->Close();
}

void TContextWrapper::Connect(std::string endpointString, py::object handler) {
    TSocketAddress endpoint = Context_->Resolve(endpointString);
    TSocketHandlePtr handle = Context_->Loop->MakeTcp();
    handle->Connect(endpoint, [handler](TSocketHandlePtr handle) {
        handler(TSocketHandleWrapper(std::move(handle)));
    });
}
