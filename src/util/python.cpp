#include <pybind11/eval.h>

#include <util/python.h>

py::object PyEvalFile(const std::string& path) {
    py::object module = py::dict();
    module["__builtins__"] = PyEval_GetBuiltins();
    py::eval_file(path, module);
    return module;
}

void PyEvalSource(const std::string& source, py::object globals) {
    globals["__builtins__"] = PyEval_GetBuiltins();
    py::exec(source, globals);
}
