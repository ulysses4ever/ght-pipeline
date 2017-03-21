#pragma once

#include <string>
#include <sstream>
#include <ctime>
#include <iomanip>

/** Shorthand for converting different types to string as long as they support the std::ostream << operator.
 */
#define STR(WHAT) static_cast<std::stringstream&>(std::stringstream() << WHAT).str()



/** Escapes the string in MySQL fashion.
 */
inline std::string escape(std::string const & from, bool quote = true) {
    std::string result = "";
    if (quote)
        result += "\"";
    for (char c : from) {
        switch (c) {
            case 0:
                result += "\\0";
                break;
            case '\'':
            case '"':
            case '%':
            case '_':
                result += "\\";
                result += c;
                break;
            case '\\':
                result += "\\\\";
                break;
            case '\b':
                result += "\\b";
                break;
            case '\n':
                result += "\\n";
                break;
            case '\r':
                result += "\\r";
                break;
            case '\t':
                result += "\\t";
                break;
            case 26:
                result += "\\Z";
                break;
            default:
                result += c;
        }
    }
    if (quote)
        result += "\"";
    return result;
}


// TODO this is done by timer
inline int timestamp() {
    return std::time(nullptr);
}


class Bytes {
public:
    Bytes():
        value_(0) {
    }

    Bytes(long value):
        value_(value) {
    }

    operator long () {
        return value_;
    }

    Bytes & operator += (long x) {
        value_ += x;
        return *this;
    }

private:
    friend std::ostream & operator << (std::ostream & s, Bytes const & b) {
        std::stringstream ss;
        ss << std::fixed << std::setprecision(2);
        if (b.value_ < 1024) {
            ss << b.value_;
        } else {
            char const * x = "KMGTP";
            int i = 0;
            double bb = b.value_ / 1024;
            while (bb > 1024 and i < 4) {
                bb /= 1024;
                ++i;
            }
            ss << std::setprecision(2) << bb << x[i];
        }
        s << ss.str();
        return s;
    }

    long value_;
};













