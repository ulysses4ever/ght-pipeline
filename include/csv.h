#pragma once

#include <fstream>
#include <vector>
#include <string>
#include <stdexcept>

#include "utils.h"



/** Simple class that allows reading a csv file line by line.
 */
class CSVParser {
public:

    static char const DELIMITER = ',';
    static char const QUOTE = '"';
    static char const ESCAPE = '\\';

    CSVParser(std::string const & filename):
        filename_(filename) {
    }

    bool operator == (CSVParser const & other) const {
        return filename_ == other.filename_;
    }

    bool operator != (CSVParser const & other) const {
        return filename_ != other.filename_;
    }

    /** Iterator actually opens the file and parses its rows.
     */
    class Iterator {
    public:

        bool operator == (Iterator const & other) const;
        bool operator != (Iterator const & other) const;

        Iterator & operator ++ () {
            parseRow();
        }

        std::vector<std::string> const & operator *() const {
            return row_;
        }

        std::vector<std::string> & operator *() {
            return row_;
        }

        
        Iterator(Iterator const &) = delete; // no copy constructor, because of the embedded stream
        Iterator(Iterator &&) = default; // move constructor is fine


        ~Iterator() {
            delete f_;
        }

    private:
        friend class CSVParser;

        Iterator(CSVParser const * parser, bool atEnd = false);

        bool eol() {
            return pos_ >= line_.size();
        }

        char top() {
            return line_[pos_];
        }

        char pop() {
            char result = line_[pos_];
            if (pos_ < line_.size())
                ++pos_;
            return result;
        }

        void getline() {
            if (not std::getline(*f_, line_))
                line_ = "";
            pos_ = 0;
        }

        void skipWhitespace() {
            while ((top() == ' ' or top() == '\t') and not eol())
                pop();
        }

        /** Parses next row in the CSV file. */
        void parseRow() {
            row_.clear();
            getline();
            if (f_->eof() and line_.empty()) {
                lineCount_ = 0;
                return;
            }
            while (not eol()) {
                skipWhitespace();
                std::string col = "";
                if (top() == QUOTE) {
                    pop(); // the quote itself
                    while (not eol() and top() != QUOTE) {
                        if (top() == ESCAPE) {
                            pop();
                            if (eol()) {
                                getline();
                                col += '\n';
                                continue;
                            }
                        }
                        col += pop();
                    }
                    if (top() != QUOTE)
                        throw std::invalid_argument(STR("Unterminated end of line, column " << row_.size() + 1 << ", line " << lineCount_));
                    pop(); // the quote
                } else {
                    // not a quote, parse till the end of line, or delimiter
                    while (top() != DELIMITER and not eol())
                        col += pop();
                }
                // we have the column
                row_.push_back(col);
                if (top() == DELIMITER) {
                    pop();
                    continue;
                }
                // if no delimiter, end
                break;
            }
            ++lineCount_;
        }

        CSVParser const & p_;

        // we need a pointer so that we can create a move constructor
        std::ifstream * f_;
        std::string line_;

        unsigned pos_;
        unsigned lineCount_;

        std::vector<std::string> row_;


    };

    Iterator begin() const {
        return Iterator(this);
    }

    Iterator end() const {
        return Iterator(this, true);
    }
private:

    std::string filename_;
};


inline CSVParser::Iterator::Iterator(CSVParser const * parser, bool atEnd):
    p_(*parser),
    pos_(0),
    lineCount_(0),
    f_(nullptr) {
    if (not atEnd) {
        // open the file
        f_ = new std::ifstream();
        f_->open(p_.filename_);
        if (not f_->good()) {
            delete f_;
            throw std::ios_base::failure(STR("Unable to open file " << p_.filename_));
        } 
        // read in the first row
        parseRow();
    }
}

inline bool CSVParser::Iterator::operator == (Iterator const & other) const {
    if (p_ != other.p_)
        return false;
    if (lineCount_ != other.lineCount_)
        return false;
    return true;
}

inline bool CSVParser::Iterator::operator != (Iterator const & other) const {
    if (p_ != other.p_)
        return true;
    if (lineCount_ != other.lineCount_)
        return true;
    return false;
}



