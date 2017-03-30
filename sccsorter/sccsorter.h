#pragma once

/** SourcererCC requires its input to have ascending file ids.

  Iternally, merge sort is used so that memory is not a limit, also we optimistically assume that most of the tokenizedFile will be sorted.
 */
class SccSorter {
public:
    static bool Verify(std::string const & filename) {
        std::cout << "verifying " << filename << "..." << std::endl;
        CSVParser p(filename);
        long lastId = -1;
        for (auto row : p) {
            long id = std::stol(row[1]);
            if (id <= lastId) {
                std::cout << "  lastId " << lastId << ", id " << id << std::endl;
                std::cout << "FAILED." << std::endl;
                return false;
            }
            lastId = id;
        }
        std::cout << "PASSED." << std::endl;
        return true;
    }

private:
};
