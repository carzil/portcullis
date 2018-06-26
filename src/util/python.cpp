#include <pybind11/eval.h>

#include "util/python.h"

py::object PyEvalFile(const std::string& path) {
    py::object module = py::dict();
    module["__builtins__"] = PyEval_GetBuiltins();
    py::eval_file(path, module);
    return module;
}
