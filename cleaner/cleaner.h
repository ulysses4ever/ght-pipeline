#pragma once

#include <iostream>
#include <string>
#include <unordered_set>

#include "include/csv.h"
#include "include/worker.h"
#include "include/filesystem.h"
#include "include/timer.h"

#include "ght/settings.h"



class Cleaner : public Worker<Cleaner, std::string> {
public:

    static void LoadPreviousRun() {
        if (not Settings::General::Incremental)
            return;
        if (not isFile(OutputFilename())) {
            std::cout << "No previous run found" << std::endl;
        } else {
            std::cout << "Loading previous run" << std::endl;
            CSVParser p(OutputFilename());
            for (auto row : p)
                projects_.insert(row[0]);
            std::cout << "    " << projects_.size() << " existing projects added.";
        }
    }

    static void FeedFrom(std::vector<std::string> const & inputs) {
        for (auto i : inputs) {
            std::cout << i << std::endl;
            Schedule(i);
        }
    }

    static void Finalize() {
        // write the stamp with number of files
        std::ofstream stamp = CheckedOpen(STR(Settings::General::Target << "/runs_cleaner.csv"), Settings::General::Incremental);
        // the stamp contains time, total number of projects
        stamp << Timer::SecondsSinceEpoch() << ","
              << skipped_ << ","
              << added_ << ","
              << total_ << ","
              << TotalTime() << std::endl;
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

    static std::string OutputFilename() {
        return STR(Settings::General::Target << "/input.csv");
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

    static bool IsValidLanguage(std::string const & language) {
        if (Settings::Cleaner::AllowedLanguages.empty())
            return true;
        for (std::string const & l : Settings::Cleaner::AllowedLanguages)
            if (l == language)
                return true;
        return false;
    }

    void run(std::string & filename) override {
        std::ofstream outFile(OutputFilename(), Settings::General::Incremental ? (std::fstream::out | std::fstream::app) : std::fstream::out);
        CSVParser p(filename);
        for (auto row : p) {
            while (Settings::General::DebugSkip > 0) {
                --Settings::General::DebugSkip;
                continue;
            }
            if (Settings::General::DebugLimit != -1 and total_ >= Settings::General::DebugLimit)
                break;
            if (not IsDeleted(row)) {
                if (Settings::Cleaner::AllowForks or not IsForked(row)) {
                    if (IsValidLanguage(Language(row))) {
                        std::string url = RelativeUrl(row);
                        // store the project to the outfile only if we haven't yet seen the url
                        if (projects_.insert(url).second) {
                            outFile << url << std::endl;
                            ++added_;
                        } else {
                            ++skipped_;
                        }
                    }
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







