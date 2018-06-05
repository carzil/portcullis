#pragma once

#include <unordered_map>

class TResource {
public:
    TResource() = default;
    TResource(const char* data, size_t size)
        : Data_(data)
        , Size_(size)
    {
    }

    const char* Data() const {
        return Data_;
    }

    size_t Size() const {
        return Size_;
    }
private:
    const char* Data_ = nullptr;
    size_t Size_ = 0;
};

class TResourceManager {
public:
    void Register(const std::string& name, const char* data, size_t size) {
        Resources_[name] = TResource(data, size);
    }

    TResource Get(const std::string& name) {
        auto it = Resources_.find(name);
        if (it == Resources_.end()) {
            return {};
        } else {
            return it->second;
        }
    }

private:
    std::unordered_map<std::string, TResource> Resources_;
};


extern TResourceManager GlobalResourceManager;

struct TResourceRegisterer {
    inline TResourceRegisterer(const std::string& name, const char* data, size_t size) {
        GlobalResourceManager.Register(name, data, size);
    }
};
