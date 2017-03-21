#include "hash.h"

#include "hashes/md5.h"

namespace {
    unsigned char fromHex(char what) {
        (what >= 'a') ? (what - 'a') + 10 : what - '0';
    }
}

Hash Hash::FromString(std::string const & from) {
    Hash result;
    unsigned char * x = reinterpret_cast<unsigned char *>(&result);
    for (unsigned i = 0; i < 16; ++i)
        x[i] = fromHex(from[i * 2]) * 16 + fromHex(from[i * 2 + 1]);
    return result;
}


Hash Hash::Calculate(std::string const & from) {
    MD5 md5;
    md5.add(from.c_str(), from.size());
    Hash result;
    md5.getHash(reinterpret_cast<unsigned char*>(& result));
    return result;
}
