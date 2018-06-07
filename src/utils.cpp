#include <unistd.h>
#include <linux/limits.h>
#include "utils.h"

std::string AbsPath(const std::string& path) {
    char resolved[PATH_MAX];
    realpath(path.c_str(), resolved);
    return std::string(resolved);
}
