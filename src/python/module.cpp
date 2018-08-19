#include "module.h"

#include <core/buffer.h>
#include <core/context.h>
#include <coro/tcp_handle.h>

#include <util/network/address.h>
#include <util/python.h>
#include <util/resource.h>

#include <python/wrappers.h>

void LoadPortcullisSubModule(py::module& m, const char* submoduleName, const std::string& submoduleResName) {
    TResource helpersSource = GlobalResourceManager.Get(submoduleResName);
    std::string source(helpersSource.Data(), helpersSource.Size());
    py::module submodule = m.def_submodule(submoduleName);
    PyEvalSource(source, submodule.attr("__dict__"));
}

void InitPortcullisModule(py::module& m) {
    py::module core = m.def_submodule("core");

    py::class_<TSocketAddress>(core, "SocketAddress")
        .def("host", &TSocketAddress::Host)
        .def("port", &TSocketAddress::Port);

    py::class_<TTcpHandleWrapper>(core, "TcpHandle")
        .def("create", &TTcpHandleWrapper::Create)
        .def("connect", &TTcpHandleWrapper::Connect)
        .def("read", &TTcpHandleWrapper::Read)
        .def("read_exactly", &TTcpHandleWrapper::ReadExactly)
        .def("write", &TTcpHandleWrapper::Write)
        .def("write_all", &TTcpHandleWrapper::WriteAll)
        .def("transfer", &TTcpHandleWrapper::Transfer)
        .def("transfer_all", &TTcpHandleWrapper::TransferAll)
        .def("close", &TTcpHandleWrapper::Close);

    py::class_<TContext, TContextPtr>(core, "Context");

    core.def("resolve", Resolve);

    py::module helpers = m.def_submodule("helpers");
    LoadPortcullisSubModule(m, "helpers", "helpers.py");
}
