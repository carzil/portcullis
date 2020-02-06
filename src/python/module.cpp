#include "module.h"

#include <pybind11/functional.h>
#include <pybind11/stl.h>
#include <pybind11/stl_bind.h>

#include <core/buffer.h>
#include <core/context.h>

#include <handles/tcp.h>
#include <handles/http.h>

#include <regexp/database.h>
#include <regexp/matchers.h>

#include <util/network/address.h>
#include <util/python.h>
#include <util/resource.h>

#include <python/wrappers.h>

PYBIND11_MAKE_OPAQUE(std::unordered_map<std::string, std::string>);

void LoadPortcullisSubModule(py::module& m, const char* submoduleName, const std::string& submoduleResName) {
    TResource helpersSource = GlobalResourceManager.Get(submoduleResName);
    std::string source(helpersSource.Data(), helpersSource.Size());
    py::module submodule = m.def_submodule(submoduleName);
    PyEvalSource(source, submodule.attr("__dict__"));
}

void InitCoreModule(py::module& core) {
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
        .def("transfer_exactly", &TTcpHandleWrapper::TransferExactly)
        .def("close", &TTcpHandleWrapper::Close);

    py::class_<TContextWrapper>(core, "Context");

    core.def("resolve", ResolveV46);
    core.def("resolve_v4", ResolveV4);
    core.def("resolve_v6", ResolveV6);
}

void InitHttpModule(py::module& http) {
    py::class_<THttpRequest>(http, "HttpRequest")
        .def_readwrite("method", &THttpRequest::Method)
        .def_readwrite("url", &THttpRequest::Url)
        .def_readwrite("minor_version", &THttpRequest::MinorVersion)
        .def_readwrite("headers", &THttpRequest::Headers);

    py::class_<THttpResponse>(http, "HttpResponse")
        .def_readwrite("status", &THttpResponse::Status)
        .def_readwrite("reason", &THttpResponse::Reason)
        .def_readwrite("minor_version", &THttpResponse::MinorVersion)
        .def_readwrite("headers", &THttpResponse::Headers);

    py::class_<THttpHandleWrapper>(http, "HttpHandle")
        .def(py::init<TTcpHandleWrapper>())
        .def("read_request", &THttpHandleWrapper::ReadRequest)
        .def("read_response", &THttpHandleWrapper::ReadResponse)
        .def("write_request", &THttpHandleWrapper::WriteRequest)
        .def("write_response", &THttpHandleWrapper::WriteResponse)
        .def("transfer_body", py::overload_cast<THttpHandleWrapper&, const THttpRequest&>(&THttpHandleWrapper::TransferBody))
        .def("transfer_body", py::overload_cast<THttpHandleWrapper&, const THttpResponse&>(&THttpHandleWrapper::TransferBody))
        .def("close", &THttpHandleWrapper::Close);
}

void BindMap(py::module& m) {
    using TMap = std::unordered_map<std::string, std::string>;
    py::class_<TMap>(m, "UnorderedMapStringString")
        .def(py::init<>())
        .def("get", [](const TMap& map, const std::string& value) -> py::object {
            auto it = map.find(value);
            if (it != map.end()) {
                return py::str(it->second);
            }
            return py::none();
        })
        .def("__setitem__", [](TMap& map, const std::string& key, const std::string& value) {
            auto it = map.find(key);
            if (it != map.end()) {
                it->second = value;
            } else {
                map.emplace(key, value);
            }
        })
        .def("__getitem__", [](const TMap& map, const std::string& key) {
            auto it = map.find(key);
            if (it == map.end()) {
                throw py::key_error();
            }
            return it->second;
        })
        .def("__bool__", [](const TMap& m) -> bool {
            return !m.empty();
        })
        .def("__iter__", [](TMap& m) {
            return py::make_key_iterator(m.begin(), m.end());
        }, py::keep_alive<0, 1>())
        .def("items", [](TMap& m) {
            return py::make_iterator(m.begin(), m.end());
        }, py::keep_alive<0, 1>())
        .def("__delitem__", [](TMap& m, const std::string& key) {
               auto it = m.find(key);
               if (it == m.end()) {
                   throw py::key_error();
               }
               m.erase(it);
        });
}

void InitReModule(py::module& m) {
    m.attr("CASELESS") = py::int_(HS_FLAG_CASELESS);
    m.attr("DOTALL") = py::int_(HS_FLAG_DOTALL);
    m.attr("MULTILINE") = py::int_(HS_FLAG_MULTILINE);
    m.attr("SINGLEMATCH") = py::int_(HS_FLAG_SINGLEMATCH);
    m.attr("ALLOWEMPTY") = py::int_(HS_FLAG_ALLOWEMPTY);
    m.attr("UTF8") = py::int_(HS_FLAG_UTF8);
    m.attr("UCP") = py::int_(HS_FLAG_UCP);
    m.attr("PREFILTER") = py::int_(HS_FLAG_PREFILTER);
    m.attr("SOM_LEFTMOST") = py::int_(HS_FLAG_SOM_LEFTMOST);

    m.attr("MODE_SOM_HORIZON_SMALL") = py::int_(HS_MODE_SOM_HORIZON_SMALL);
    m.attr("MODE_SOM_HORIZON_MEDIUM") = py::int_(HS_MODE_SOM_HORIZON_MEDIUM);
    m.attr("MODE_SOM_HORIZON_LARGE") = py::int_(HS_MODE_SOM_HORIZON_LARGE);

    py::class_<TRegexpDef>(m, "RegexpDef")
        .def(py::init<std::string, unsigned int>());

    py::class_<TStreamRegexpDatabase>(m, "StreamRegexpDatabase")
        .def(py::init<std::vector<TRegexpDef>, unsigned int>(), py::arg("regexps"), py::arg("mode") = 0);

    py::class_<TStreamRegexpMatcher>(m, "StreamRegexpMatcher")
        .def(py::init<TStreamRegexpDatabase, TStreamRegexpMatcher::TMatchCallback>())
        .def("scan", [](TStreamRegexpMatcher& matcher, std::string_view chunk) {
            matcher.Scan(chunk);
        });
}

void InitPortcullisModule(py::module& m) {
    BindMap(m);

    py::module core = m.def_submodule("core");
    InitCoreModule(core);

    py::module http = m.def_submodule("http");
    InitHttpModule(http);

    py::module _re = m.def_submodule("_re");
    InitReModule(_re);

    py::module helpers = m.def_submodule("helpers");
    LoadPortcullisSubModule(m, "helpers", "helpers.py");

    py::module re = m.def_submodule("re");
    LoadPortcullisSubModule(m, "re", "re.py");
}

