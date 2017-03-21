#include <memory>

#include "utils.h"
#include "exec.h"


#include <iostream>

bool exec(std::string const & what, std::string const & path) {
    std::string cmd = STR("cd \"" << path << "\" && " << what);
    return system(cmd.c_str()) == EXIT_SUCCESS;
}

std::string execAndCapture(std::string const & cmd, std::string const & path) {
    char buffer[1024];
    std::string result = "";
    std::string what = STR("cd \"" << path << "\" && " << cmd << " 2>&1");
    FILE * pipe = popen(what.c_str(), "r");
    if (not pipe)
        throw std::ios_base::failure(STR("Unable to execute command " << cmd));
    try {
        while (not feof(pipe)) {
            if (fgets(buffer, 1024, pipe) != nullptr)
                result += buffer;
        }
        pclose(pipe);
        return result;
    } catch (...) {
        pclose(pipe);
        throw;
    }
}

bool execAndCapture(std::string const & cmd, std::string const & path, std::string & output) {
    char buffer[1024];
    std::string what = STR("cd \"" << path << "\" && " << cmd << " 2>&1");
    FILE * pipe = popen(what.c_str(), "r");
    if (not pipe)
        throw std::ios_base::failure(STR("Unable to execute command " << cmd));
    output.clear();
    try {
        while (not feof(pipe)) {
            if (fgets(buffer, 1024, pipe) != nullptr)
                output += buffer;
        }
        return pclose(pipe) == 0;
    } catch (...) {
        pclose(pipe);
        throw;
    }
}

