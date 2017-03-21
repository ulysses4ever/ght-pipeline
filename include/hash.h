#pragma once

#include <cstdint>
#include <string>
#include <ostream>








/** A compressed way of storing md5 hashes so that they take less memory and are easier to compare.
 */
struct Hash {
    uint64_t first;
    uint64_t second;

    Hash():
        first(0),
        second(0) {
    }

    /** Creates hash from its hexadecimal representation (reverse to the << operator).
     */
    static Hash FromString(std::string const & from);

    /** Calculates hash of given string.
     */
    static Hash Calculate(std::string const & from);

    Hash & operator = (Hash const & other) = default;

    /** Equality. */
    bool operator == (Hash const & other) const {
        return first == other.first and second == other.second;
    }

    /** Inequality. */
    bool operator != (Hash const & other) const {
        return first != other.first or second != other.second;
    }

    /** Outputs the hash to a string in hex format.
     */
    friend std::ostream & operator << (std::ostream & s, Hash const & h) {
        static const char dec2hex[16+1] = "0123456789abcdef";
        unsigned char const * x = reinterpret_cast<unsigned char const *>(&h);
        for (int i = 0; i < 16; ++i) {
            s << dec2hex[(x[i] >> 4) & 15];
            s << dec2hex[(x[i]) & 15];
        }
        return s;
    }
};

namespace std {
    /** So that Hash can be key in std containers.
     */
    template<>
    struct hash<::Hash> {

        std::size_t operator()(::Hash const & h) const {
            return std::hash<uint64_t>{}(h.first) + std::hash<uint64_t>{}(h.second);
        }

};
}
