#include <core/context.h>

#include <pybind11/eval.h>
#include <util/python.h>

TSocketBufferPtr TContext::AllocBuffer(size_t size) {
    return std::make_shared<TSocketBuffer>(size);
}

template <typename T>
static inline T ReadFromConfig(py::object pyConfig, const std::string& key) {
    if (!pyConfig.contains(key)) {
        throw TException() << "'" << key << "' is not set in config";
    }
    return pyConfig[key.c_str()].cast<T>();
}

TConfig ReadConfigFromFile(std::string filename) {
    TConfig config;
    py::object pyConfig = PyEvalFile(filename);
    config.Host = ReadFromConfig<std::string>(pyConfig, "host");
    config.Port = ReadFromConfig<std::string>(pyConfig, "port");
    config.Backlog = ReadFromConfig<size_t>(pyConfig, "backlog");
    config.HandlerFile = ReadFromConfig<std::string>(pyConfig, "handler_file");
    config.BackendIp = ReadFromConfig<std::string>(pyConfig, "backend_ip");
    config.BackendIpv6 = ReadFromConfig<std::string>(pyConfig, "backend_ipv6");
    config.BackendPort = ReadFromConfig<std::string>(pyConfig, "backend_port");
    config.Protocol = ReadFromConfig<std::string>(pyConfig, "protocol");
    config.CoroutineStackSize = ReadFromConfig<size_t>(pyConfig, "coroutine_stack_size");
    return config;
}
