#include <pybind11/pybind11.h>

namespace py = pybind11;

py::object PyEvalFile(const std::string& path);
void PyEvalSource(const std::string& source, py::object globals);
