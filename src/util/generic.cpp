#include <unistd.h>
#include <linux/limits.h>
#include "util/generic.h"

std::string AbsPath(const std::string& path) {
    char resolved[PATH_MAX];
    realpath(path.c_str(), resolved);
    return std::string(resolved);
}
