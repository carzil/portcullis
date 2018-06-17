#include "module.h"

void InitPortcullisModule(py::module& m) {
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
        .def("connect", &TContextWrapper::Connect);

    py::class_<TSplicer, TSplicerPtr>(m, "Splicer")
        .def(py::init<TContextWrapper, TSocketHandleWrapper, TSocketHandleWrapper>())
        .def("start", &TSplicer::Start, py::arg("on_end"));
}
