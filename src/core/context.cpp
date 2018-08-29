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
    config.Name = ReadFromConfig<std::string>(pyConfig, "name");
    config.Host = ReadFromConfig<std::string>(pyConfig, "host");
    config.Port = ReadFromConfig<std::string>(pyConfig, "port");
    config.Backlog = ReadFromConfig<size_t>(pyConfig, "backlog");
    config.HandlerFile = ReadFromConfig<std::string>(pyConfig, "handler_file");
    config.BackendHost = ReadFromConfig<std::string>(pyConfig, "backend_host");
    config.BackendPort = ReadFromConfig<std::string>(pyConfig, "backend_port");
    config.Protocol = ReadFromConfig<std::string>(pyConfig, "protocol");
    config.AllowIpv6 = ReadFromConfig<bool>(pyConfig, "allow_ipv6");
    config.Ipv6Only = ReadFromConfig<bool>(pyConfig, "ipv6_only");
    return config;
}
