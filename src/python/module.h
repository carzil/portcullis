#pragma once

#include <pybind11/embed.h>

#include "python/wrappers.h"
#include "shield/splicer.h"

namespace py = pybind11;

void InitPortcullisModule(py::module& m);

PYBIND11_EMBEDDED_MODULE(portcullis, m) {
    InitPortcullisModule(m);
}
