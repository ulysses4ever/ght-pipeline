
#include <sys/types.h>
#include <sys/stat.h>
#include "utils.h"
#include "filesystem.h"

bool isDirectory(std::string const & path) {
    struct stat s;
    if (lstat(path.c_str(),&s) == 0)
        return S_ISDIR(s.st_mode);
    return false;
}

bool isFile(std::string const & path) {
    struct stat s;
    if (lstat(path.c_str(),&s) == 0)
        return S_ISREG(s.st_mode);
    return false;
}

void createPath(std::string const & path) {
    if (system(STR("mkdir -p " << path).c_str()) != EXIT_SUCCESS)
        throw std::ios_base::failure(STR("Unable to create path " << path));
}

void createPathIfMissing(std::string const & path) {
    if (not isDirectory(path))
        createPath(path);
}


void deletePath(std::string const & path) {
    if (system(STR("rm -rf " << path).c_str()) != EXIT_SUCCESS)
       throw std::ios_base::failure(STR("Unable to delete path " << path));
}
