#include <pybind11/eval.h>

#include "core/context.h"
#include "shield/splicer.h"

#include "util/python.h"


TSocketAddress TContext::Resolve(const std::string& addressString) {
    auto it = CachedAddrs_.find(addressString);
    if (it != CachedAddrs_.end()) {
        return it->second;
    }

    size_t protoDelimPos = addressString.find("://");
    if (protoDelimPos == std::string::npos) {
        throw TException() << "malformed resolve-string: '" << addressString << "'";
    }

    std::string proto = addressString.substr(0, protoDelimPos);

    size_t hostDelimPos = addressString.find(":", protoDelimPos + 3);
    if (hostDelimPos == std::string::npos) {
        throw TException() << "malformed resolve-string: '" << addressString << "'";
    }

    std::string host = addressString.substr(protoDelimPos + 3, hostDelimPos - (protoDelimPos + 3));
    std::string service = addressString.substr(hostDelimPos + 1);

    TSocketAddress addr = GetAddrInfo(host, service, false, proto)[0];

    CachedAddrs_.insert({ addressString, addr });
    return addr;
}

void TContext::Finalize() {
    Loop->Cancel(CleanupHandle);
}

void TContext::Cleanup() {

}

void TContext::StartSplicer(TSplicerPtr splicer) {
    if (splicer->Id() > ActiveSplicers_.size()) {
        ActiveSplicers_.resize(splicer->Id() + 1);
    }
    ActiveSplicers_[splicer->Id()] = splicer;
    splicer->Start();
}

void TContext::StopSplicer(TSplicerPtr splicer) {
    FinishingSplicers_.push_back(splicer);
    ActiveSplicers_[splicer->Id()] = nullptr;
}

TContext::~TContext() {
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
    return config;
}
