#pragma once

#include <cstdint>
#include <string>
#include <ostream>
#include <cassert>



template<unsigned BYTES>
struct Hash {

    static_assert(BYTES % 4 == 0, "Invalid hash size");

    Hash() = default;

    Hash(std::string const & hex) {
        assert(hex.size() == BYTES * 2);
        for (unsigned i = 0; i < BYTES; ++i)
            data_[i] = FromHex(hex[i * 2]) * 16 + FromHex(hex[i * 2 + 1]);
    }

    bool operator == (Hash<BYTES> const & other) const {
        for (unsigned i = 0; i < BYTES; ++i)
            if (data_[i] != other.data_[i])
                return false;
        return true;
    }

    bool operator != (Hash<BYTES> const & other) const {
        for (unsigned i = 0; i < BYTES; ++i)
            if (data_[i] != other.data_[i])
                return true;
        return false;
    }

private:
    friend class std::hash<::Hash<BYTES>>;

    friend std::ostream & operator << (std::ostream & s, Hash<BYTES> const &h) {
        static const char dec2hex[16+1] = "0123456789abcdef";
        for (int i = 0; i < BYTES; ++i) {
            s << dec2hex[(h.data_[i] >> 4) & 15];
            s << dec2hex[(h.data_[i]) & 15];
        }
        return s;
    }

    unsigned char data_[BYTES];

    static unsigned char FromHex(char what) {
        return (what >= 'a') ? (what - 'a') + 10 : what - '0';
    }
};

namespace std {
    /** So that Hash can be key in std containers.
     */
    template<unsigned BYTES>
    struct hash<::Hash<BYTES>> {

        std::size_t operator()(::Hash<BYTES> const & h) const {
            std::size_t result = 0;
            unsigned i = 0;
            for (; i < BYTES - 8; i += 8)
                result += std::hash<uint64_t>()(* reinterpret_cast<uint64_t const*>(h.data_+i));
            for (; i < BYTES; ++i)
                result += std::hash<char>()(h.data_[i]);
            return result;
        }
    };
}


typedef Hash<16> MD5;
typedef Hash<20> SHA1;
