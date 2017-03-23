#pragma once

#include <iostream>
#include <string>
#include <unordered_set>

#include "include/csv.h"
#include "include/worker.h"
#include "include/filesystem.h"
#include "include/timer.h"

#include "ght/settings.h"

class CleanerAllLang : public Worker<CleanerAllLang, std::string> {
public:

    static void FeedFrom(std::vector<std::string> const & inputs) {
        for (auto i : inputs) {
            std::cout << i << std::endl;
            Schedule(i);
        }
    }

    static long SkippedProjects() {
        return skipped_;
    }

    static long AddedProjects() {
        return added_;
    }

    static long TotalProjects() {
        return total_;
    }

    static ProgressReporter::Feeder GetReporterFeeder() {
        return [](ProgressReporter & p, std::ostream & s) {
            p.done = added_;
            if (AllDone())
                p.allDone = true;
            s << "added: " << std::left << std::setw(8) << added_
              << "skipped: " << std::setw(8) << skipped_
              << "total: " << std::setw(8) << total_ << std::endl;
        };
    }

private:

    static bool IsForked(std::vector<std::string> const & row) {
        return row[7] != "\\N";
    }

    static bool IsDeleted(std::vector<std::string> const & row) {
        return row[8] != "0";
    }

    static std::string const & Language(std::vector<std::string> const & row) {
        return row[5];
    }

    static std::string RelativeUrl(std::vector<std::string> const & row) {
        return row[1].substr(29);
    }

    void run(std::string & filename) override {
        std::ofstream outFile(Settings::CleanerAllLang::OutputFile, std::fstream::out);
        CSVParser p(filename);
        for (auto row : p) {
            if (total_ == 0) { // skip first line
                ++total_;
                continue;
            }
            if (not IsDeleted(row)) {
                std::string url = RelativeUrl(row);
                // store the project to the outfile only if we haven't yet seen the url
                if (projects_.insert(url).second) {
                    outFile << escape(url) << ","
                            << Language(row) << ","
                            << (IsForked(row) ? "1" : "0" ) << std::endl;
                    ++added_;
                } else {
                    ++skipped_;
                }
            }
            ++total_;
        }
    }

    static long skipped_;
    static long added_;
    static long total_;

    static std::unordered_set<std::string> projects_;

};
