#pragma once

#include <fstream>
#include <string>
#include <vector>


inline std::ofstream CheckedOpen(std::string const & filename, bool append = false) {
    std::ofstream result(filename, append ? (std::fstream::app | std::fstream::out) : std::fstream::out);
    if (not result.good())
        throw std::ios_base::failure(STR("Unable to open file " << filename));
    return result;
}

inline std::ifstream CheckedRead(std::string const & filename) {
    std::ifstream result(filename);
    if (not result.good())
        throw std::ios_base::failure(STR("Unable to open file " << filename));
    return result;
}



inline std::string LoadEntireFile(std::string const & filename) {
    using namespace std;
    ifstream f(filename, ios::in | ios::binary | ios::ate);
    ifstream::pos_type fSize = f.tellg();
    f.seekg(0, ios::beg);
    vector<char> bytes(fSize);
    f.read(bytes.data(), fSize);
    return string(bytes.data(), fSize);
}


bool isDirectory(std::string const & path);

bool isFile(std::string const & path);

void createPath(std::string const & path);


void createPathIfMissing(std::string const & path);


/** Basically a rm -rf on the given path.
 */
void deletePath(std::string const & path);

