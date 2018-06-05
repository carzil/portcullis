#pragma once

#include <iostream>
#include <exception>
#include <sstream>
#include <cassert>


class TException : public std::exception {
public:
    virtual const char* what() const noexcept {
        return Error_.c_str();
    }

    template<class T>
    void Append(const T& t) {
        std::stringstream ss;
        ss << Error_ << t;
        Error_ = ss.str();
    }

private:
    std::string Error_;
};

template<class T>
inline TException&& operator<<(TException&& e, const T& t) {
    e.Append(t);
    return std::forward<TException>(e);
};


#ifdef _DEBUG_
#define ASSERT(a) assert(a)
#else
#define ASSERT(a)
#endif
