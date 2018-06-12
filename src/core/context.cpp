#include "core/context.h"
#include "shield/splicer.h"


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

TSplicerPtr TContext::MakeSplicer(TSocketHandlePtr client, TSocketHandlePtr backend) {
    TSplicerPtr splicer(new TSplicer(shared_from_this(), std::move(client), std::move(backend)));
    if (splicer->Id() >= ActiveSplicers_.size()) {
        ActiveSplicers_.resize(splicer->Id() + 1);
    }
    ActiveSplicers_[splicer->Id()] = splicer;
    return splicer;
}

void TContext::Cleanup() {
    InactiveSplicers_.clear();
}

void TContext::FinishSplicer(TSplicer* splicer) {
    TSplicerPtr ptr = ActiveSplicers_[splicer->Id()];
    InactiveSplicers_.push_back(std::move(ptr));
    ActiveSplicers_[splicer->Id()] = nullptr;
}
