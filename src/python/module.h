#pragma once

#include <pybind11/embed.h>

#include "python/wrappers.h"

namespace py = pybind11;

PYBIND11_EMBEDDED_MODULE(portcullis, m) {
    py::class_<TSocketAddress>(m, "SocketAddress")
        .def("host", &TSocketAddress::Host)
        .def("port", &TSocketAddress::Port);

    py::class_<TSocketHandleWrapper>(m, "SocketHandle")
        .def("address", &TSocketHandleWrapper::Address)
        .def("read", &TSocketHandleWrapper::Read)
        .def("write", &TSocketHandleWrapper::Write)
        .def("close", &TSocketHandleWrapper::Close);

    py::class_<TContextWrapper>(m, "Context")
        .def("make_tcp", &TContextWrapper::MakeTcp)
        .def("splice", &TContextWrapper::Splice)
        .def("connect", &TContextWrapper::Connect);

    py::class_<TSplicer, TSplicerPtr>(m, "Splicer");
}
