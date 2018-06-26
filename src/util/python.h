#include <pybind11/pybind11.h>

namespace py = pybind11;

py::object PyEvalFile(const std::string& path);
