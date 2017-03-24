#pragma once

#include <string>
#include <unordered_set>
#include <unordered_map>
#include <vector>
#include <iostream>
#include <fstream>
#include <climits>
#include <cassert>

#include "ght/settings.h"
#include "include/filesystem.h"
#include "include/csv.h"



/** Simple merger of the old tokenizer's results.

 */
class StrideMerger {
public:

    static void Merge(std::string const & s1, std::string const &s2, std::string const & t) {
        if (ResultsReady(t)) {
            std::cout << "Results for " << t << " already exist and will not be recomputed." << std::endl;
            return;
        }
        StrideMerger m;
        //m.mergeProjects(s1, s2, t);
        //m.mergeFiles(s1, s2, t);
        //m.mergeStats(s1, s2, t);
        m.mergeTokensText(s1, s2, t);
        m.mergeTokensCount(s1, s2, t);
        m.mergeTokenizedFiles(s1, s2, t);
    }

    static std::string Merge(int start, int end) {
        if (start == end) {
            return STR(start); // and we are done
        } else {
            std::string target = STR(start << "-" << end);
            if (start + 1 == end) {
                Merge(STR(start), STR(end), target);
            } else {
                int middle = (end - start) / 2 + start;
                std::string first = Merge(start, middle);
                std::string second = Merge(middle + 1, end);
                Merge(first, second, target);
                if (middle != start)
                    DeleteResults(first);
                if (middle != end)
                    DeleteResults(second);
            }
            return target;
        }
    }

private:
    static bool ResultsReady(std::string const & suffix) {
        if (not isFile(STR(Settings::StrideMerger::Folder << "/projects_" << suffix << ".txt")))
            return false;
        if (not isFile(STR(Settings::StrideMerger::Folder << "/projects_extra" << suffix << ".txt")))
            return false;
        if (not isFile(STR(Settings::StrideMerger::Folder << "/files_" << suffix << ".txt")))
            return false;
        if (not isFile(STR(Settings::StrideMerger::Folder << "/files_extra" << suffix << ".txt")))
            return false;
        if (not isFile(STR(Settings::StrideMerger::Folder << "/stats_" << suffix << ".txt")))
            return false;
        if (not isFile(STR(Settings::StrideMerger::Folder << "/tokenized_files_" << suffix << ".txt")))
            return false;
        if (not isFile(STR(Settings::StrideMerger::Folder << "/tokens_text_" << suffix << ".txt")))
            return false;
        if (not isFile(STR(Settings::StrideMerger::Folder << "/tokens_" << suffix << ".txt")))
            return false;
        return true;
    }


    static void DeleteResults(std::string const & suffix) {
        std::cout << "Deleting intermediate results for suffix " << suffix << std::endl;
        deletePath(STR(Settings::StrideMerger::Folder << "/projects_" << suffix << ".txt"));
        deletePath(STR(Settings::StrideMerger::Folder << "/projects_extra" << suffix << ".txt"));
        deletePath(STR(Settings::StrideMerger::Folder << "/files_" << suffix << ".txt"));
        deletePath(STR(Settings::StrideMerger::Folder << "/files_extra" << suffix << ".txt"));
        deletePath(STR(Settings::StrideMerger::Folder << "/stats_" << suffix << ".txt"));
        deletePath(STR(Settings::StrideMerger::Folder << "/tokenized_files_" << suffix << ".txt"));
        deletePath(STR(Settings::StrideMerger::Folder << "/tokens_text" << suffix << ".txt"));
        deletePath(STR(Settings::StrideMerger::Folder << "/tokens_" << suffix << ".txt"));
    }

    void append(std::string const & s1, std::string const & s2, std::string const & t) {
        long l1 = 0;
        long l2 = 0;
        std::ofstream o = CheckedOpen(t);
        {
            std::cout << "  " << s1 << std::flush;
            std::ifstream i(s1);
            std::string line;
            while (std::getline(i, line)) {
                o << line << std::endl;
                ++l1;
            }
            std::cout << ", " << l1 << " records" << std::endl;
        }
        {
            std::cout << "  " << s2 << std::flush;
            std::ifstream i(s2);
            std::string line;
            while (std::getline(i, line)) {
                o << line << std::endl;
                ++l2;
            }
            std::cout << ", " << l1 << " records" << std::endl;
        }
        std::cout << "  " << (l1 + l2) << " records total" << std::endl;
    }

    /** Merging projects is easy, we just append them together.

      Also merges projects_extra.
     */
    void mergeProjects(std::string const & sone, std::string const & stwo, std::string const & target) {
        std::string s1 = STR(Settings::StrideMerger::Folder << "/projects_" << sone << ".txt");
        std::string s2 = STR(Settings::StrideMerger::Folder << "/projects_" << stwo << ".txt");
        std::string t = STR(Settings::StrideMerger::Folder << "/projects_" << target << ".txt");
        std::cout << "Merging projects.csv..." << std::endl;
        append(s1, s2, t);
        s1 = STR(Settings::StrideMerger::Folder << "/projects_extra_" << sone << ".txt");
        s2 = STR(Settings::StrideMerger::Folder << "/projects_extra_" << stwo << ".txt");
        t = STR(Settings::StrideMerger::Folder << "/projects_extra_" << target << ".txt");
        std::cout << "Merging projects_extra.csv..." << std::endl;
        append(s1, s2, t);
    }

    /** Merging files is easy too, we just append them together.

      Also merges files_extra
     */
    void mergeFiles(std::string const & sone, std::string const & stwo, std::string const & target) {
        std::string s1 = STR(Settings::StrideMerger::Folder << "/files_" << sone << ".txt");
        std::string s2 = STR(Settings::StrideMerger::Folder << "/files_" << stwo << ".txt");
        std::string t = STR(Settings::StrideMerger::Folder << "/files_" << target << ".txt");
        std::cout << "Merging files.csv..." << std::endl;
        append(s1, s2, t);
        s1 = STR(Settings::StrideMerger::Folder << "/files_extra_" << sone << ".txt");
        s2 = STR(Settings::StrideMerger::Folder << "/files_extra_" << stwo << ".txt");
        t = STR(Settings::StrideMerger::Folder << "/files_extra_" << target << ".txt");
        std::cout << "Merging files_extra.csv..." << std::endl;
        append(s1, s2, t);

    }

    /** All stats from first file survive, file hashes from second, not present in first are added.
     */
    void mergeStats(std::string const & sone, std::string const & stwo, std::string const & target) {
        std::string s1 = STR(Settings::StrideMerger::Folder << "/stats_" << sone << ".txt");
        std::string s2 = STR(Settings::StrideMerger::Folder << "/stats_" << stwo << ".txt");
        std::string t = STR(Settings::StrideMerger::Folder << "/stats_" << target << ".txt");
        std::cout << "Merging stats.csv..." << std::endl;
        std::ofstream o = CheckedOpen(t);
        std::unordered_set<std::string> fileHashes;
        long l1 = 0;
        long l2 = 0;
        {
            CSVParser p(s1);
            std::cout << "  " << s1 << std::flush;
            for (auto row: p) {
                fileHashes.insert(row[0]);
                o << row[0] << ","
                  << row[1] << ","
                  << row[2] << ","
                  << row[3] << ","
                  << row[4] << ","
                  << row[5] << ","
                  << row[6] << ","
                  << row[7] << std::endl;
                ++l1;
            }
            std::cout << ", " << l1 << " records" << std::endl;
        }
        long total = l1;
        {
            CSVParser p(s2);
            std::cout << "  " << s2 << std::flush;
            for (auto row: p) {
                if (fileHashes.find(row[0]) == fileHashes.end()) {
                    fileHashes.insert(row[0]);
                    o << row[0] << ","
                      << row[1] << ","
                      << row[2] << ","
                      << row[3] << ","
                      << row[4] << ","
                      << row[5] << ","
                      << row[6] << ","
                      << row[7] << std::endl;
                    ++total;
                }
                ++l2;
            }
            std::cout << ", " << l2 << " records" << std::endl;
        }
        std::cout << " total " << total << " records after merge" << std::endl;
    }

    /** Merges the tokenized files.

      While merging, performs also a merge sort so that the output is sorted wrt increasing file ids, which is a requirement for sourcererCC.

      tokens = id, count

      tokens_text - id, size, hash, text


     */


    void mergeTokensText(std::string const & sone, std::string const & stwo, std::string const & target) {
        std::string s1 = STR(Settings::StrideMerger::Folder << "/tokens_text_" << sone << ".txt");
        std::string s2 = STR(Settings::StrideMerger::Folder << "/tokens_text_" << stwo << ".txt");
        std::string t = STR(Settings::StrideMerger::Folder << "/tokens_text_" << target << ".txt");
        std::cout << "Merging tokens_text.csv..." << std::endl;
        std::ofstream o = CheckedOpen(t);
        std::unordered_map<std::string, long> tokens;
        long l1 = 0;
        long l2 = 0;
        long nextId = 0;
        {
            CSVParser p(s1);
            std::cout << "  " << s1 << std::flush;
            for (auto row: p) {
                long id = std::stol(row[0]);
                // this is just defensive programming, we assume the ids to be consecutive
                if (id > nextId)
                    nextId = id;
                if (tokens.find(row[2]) != tokens.end()) {
                    std::cout << "HOUSTON WE HAVE A PROBLEM" << std::endl;
                    std::cout << tokens[row[2]] << "--" << id << std::endl;
                }
                tokens.insert(std::make_pair(row[2], id));
                o << row[0] << ","
                  << row[1] << ","
                  << row[2] << ","
                  << escape(row[3]) << std::endl;
                ++l1;
            }
            std::cout << ", " << l1 << " records" << std::endl;
        }
        long total = l1;
        // now do the second one
        {
            CSVParser p(s2);
            std::cout << "  " << s2 << std::flush;
            for (auto row: p) {
                long id = std::stol(row[0]);
                auto i = tokens.find(row[2]);
                if (i == tokens.end()) {
                    translation_.insert(std::make_pair(id, ++nextId));
                    id = nextId;
                    o << id << ","
                      << row[1] << ","
                      << row[2] << ","
                      << escape(row[3]) << std::endl;
                    ++total;
                } else {
                    translation_.insert(std::make_pair(id, i->second));
                    id = i->second;
                }
                ++l2;
            }
            std::cout << ", " << l2 << " records" << std::endl;
        }
        std::cout << " total " << total << " records after merge" << std::endl;
    }

    void mergeTokensCount(std::string const & sone, std::string const & stwo, std::string const & target) {
        // let's do the counts now, defensive - not assuming the ids are sequence
        std::unordered_map<long, long> counts;
        std::string s1 = STR(Settings::StrideMerger::Folder << "/tokens_" << sone << ".txt");
        std::string s2 = STR(Settings::StrideMerger::Folder << "/tokens_" << stwo << ".txt");
        std::string t = STR(Settings::StrideMerger::Folder << "/tokens_" << target << ".txt");
        std::cout << "Merging tokens.csv..." << std::endl;
        long l1 = 0;
        long l2 = 0;
        {
            CSVParser p(s1);
            std::cout << "  " << s1 << std::flush;
            for (auto row : p) {
                long id = std::stol(row[0]);
                long count = std::stol(row[1]);
                counts[id] += count;
                ++l1;
            }
            std::cout << ", " << l1 << " records" << std::endl;
        }
        {
            CSVParser p(s2);
            std::cout << "  " << s2 << std::flush;
            for (auto row : p) {
                long id = std::stol(row[0]);
                long count = std::stol(row[1]);
                counts[id] += count;
                ++l2;
            }
            std::cout << ", " << l2 << " records" << std::endl;
        }
        std::cout << "  writing..." << std::endl;
        {
            std::ofstream o = CheckedOpen(t);
            for (auto i : counts)
                o << i.first << "," << i.second << std::endl;
        }
        std::cout << " total " << counts.size() << " records after merge" << std::endl;
    }

    void writeTokenizedFile(std::ostream & s, std::vector<std::string> const & row) {
        s << row[0] << ","
          << row[1] << ","
          << row[2] << ","
          << row[3] << ","
          << row[4] << ", @#@";
        unsigned i = 5;
        while (true) {
            s << row[i];
            if (++i == row.size())
                break;
            s << ",";
        }
        s << std::endl;
    }

    int hexChar(char x) {
        return (x >= 'a') ? ( 10 + x - 'a') : x - '0';
    }

    void translateTokenizedFile(std::vector<std::string> & row) {
        unsigned i = 5;
        while (true) {
            long id = 0;
            unsigned j = 0;
            while (row[i][j] != '@')
                id = id * 16 + hexChar(row[i][j++]);
            j += 6; // @@::@@
            long count = std::atol(row[i].c_str() + i);
            row[i] = STR(std::hex << translation_[id] << "@@::@@" << std::dec << count);
            if (++i == row.size())
                break;
        }
    }

    void mergeTokenizedFiles(std::string const & sone, std::string const & stwo, std::string const & target) {
        std::unordered_set<std::string> tokenHashes;
        std::string s1 = STR(Settings::StrideMerger::Folder << "/tokenized_files_" << sone << ".txt");
        std::string s2 = STR(Settings::StrideMerger::Folder << "/tokenized_files_" << stwo << ".txt");
        std::string t = STR(Settings::StrideMerger::Folder << "/tokenized_files_" << target << ".txt");
        std::cout << "Merging tokens.csv..." << std::endl;
        std::ofstream o = CheckedOpen(t);
        long l1 = 0;
        long l2 = 0;
        std::cout << "  " << s1 << std::endl;
        std::cout << "  " << s2 << std::endl;
        CSVParser p1(s1);
        CSVParser p2(s2);
        auto i1 = p1.begin(), e1 = p2.end();
        auto i2 = p2.begin(), e2 = p2.end();
        while (i1 != e1 and i2 != e2) {
            long index1 = (i1 == e1) ? LONG_MAX : std::stol((*i1)[1]);
            long index2 = (i2 == e2) ? LONG_MAX : std::stol((*i2)[1]);
            if (index1 < index2) {
                std::string const & h = (*i1)[4];
                if (tokenHashes.find(h) == tokenHashes.end()) {
                    tokenHashes.insert(h);
                    writeTokenizedFile(o, *i1);
                }
                ++i1;
                ++l1;
            } else if (index2 < index1) {
                std::string const & h = (*i2)[4];
                if (tokenHashes.find(h) == tokenHashes.end()) {
                    tokenHashes.insert(h);
                    translateTokenizedFile(*i2);
                    writeTokenizedFile(o, *i2);
                }
                ++i2;
                ++l2;
            } else {
                std::cout << index1 << std::endl;
                std::cout << index2 << std::endl;
                assert(false and "This is not possible");
            }
        }
        std::cout << "  " << l1 << " records in first" << std::endl;
        std::cout << "  " << l2 << " records in second" << std::endl;
        std::cout << "  " << tokenHashes.size() << " unique token hashes in output" << std::endl;
    }


    std::unordered_map<long, long> translation_;
};
