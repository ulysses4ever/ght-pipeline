#pragma once

#include <string>

bool exec(std::string const & what, std::string const & path);
std::string execAndCapture(std::string const & cmd, std::string const & path);
bool execAndCapture(std::string const & cmd, std::string const & path, std::string & output);

