#pragma once

#include <pybind11/embed.h>
namespace py = pybind11;

PYBIND11_EMBEDDED_MODULE(fast_calc, m) {
    m.def("add", [](int i, int j) {
        return i + j;
    });
}
