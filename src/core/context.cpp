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
    Logger->info("context destroyed");
}

TConfig ReadConfigFromFile(std::string filename) {
    TConfig config;
    py::object pyConfig = PyEvalFile(filename);
    config.Name = pyConfig["name"].cast<std::string>();
    config.Host = pyConfig["host"].cast<std::string>();
    config.Port = pyConfig["port"].cast<std::string>();
    config.Backlog = pyConfig["backlog"].cast<size_t>();
    config.HandlerFile = pyConfig["handler_file"].cast<std::string>();
    config.BackendHost = pyConfig["backend_host"].cast<std::string>();
    config.BackendPort = pyConfig["backend_port"].cast<std::string>();
    config.Protocol = pyConfig["protocol"].cast<std::string>();
    config.Managed = pyConfig["managed"].cast<bool>();
    return config;
}
