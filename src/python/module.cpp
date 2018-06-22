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
        .def("connect", &TContextWrapper::Connect, py::arg("endpoint"), py::arg("on_connect"))
        .def("start_splicer", &TContextWrapper::StartSplicer, py::arg("splicer"));

    py::class_<TSplicer, TSplicerPtr>(m, "Splicer")
        .def(py::init<TContextWrapper, TSocketHandleWrapper, TSocketHandleWrapper, py::object>(), py::arg("context"), py::arg("client"), py::arg("backend"), py::arg("on_end"));
}
