#pragma once

#include <mutex>
#include "include/utils.h"


//#include "include/settings.h"


class Settings {
public:
    class General {
    public:
        static std::string Target;
        static bool Incremental;
        static unsigned NumThreads;
        static long DebugLimit;
        static long DebugSkip;
        static std::vector<std::string> ApiTokens;

        static unsigned FilesPerFolder;




        static void LoadAPITokens(std::string const & from);

        static std::string const & GetNextApiToken();


    private:
        static unsigned apiTokenIndex_;
        static std::mutex apiTokenGuard_;
    };

    class Cleaner {
    public:
        static std::vector<std::string> InputFiles;
        static std::vector<std::string> AllowedLanguages;
        static bool AllowForks;
    };

    class CleanerAllLang {
    public:
        static std::string OutputFile;
    };

    class Downloader {
    public:
        static std::vector<std::string> AllowPrefix;
        static std::vector<std::string> AllowSuffix;
        static std::vector<std::string> AllowContents;
        static std::vector<std::string> DenyPrefix;
        static std::vector<std::string> DenySuffix;
        static std::vector<std::string> DenyContents;

        static bool CompressFileContents;
        static bool CompressInExtraThread;
        static int MaxCompressorThreads;
        static bool KeepRepos;

    };

    class StrideMerger {
    public:
        static std::string Folder;
    };


};

/** Takes given id and produces a path from it that would make sure the MaxFilesPerDirectory limit is not broken assuming full utilization of the ids.
*/
inline std::string IdToPath(long id, std::string const & prefix = "") {
    std::string result = "";
    // get the directory id first, which chops the ids into chunks of MaxEntriesPerDirectorySize
    long dirId = id / Settings::General::FilesPerFolder;
    // construct the path while dirId != 0
    while (dirId != 0) {
        result = STR("/" << prefix << std::to_string(dirId % Settings::General::FilesPerFolder) << result);
        dirId = dirId / Settings::General::FilesPerFolder;
    }
    return result;
}


inline bool IdPathFull(long id) {
    return ((id + 1) % Settings::General::FilesPerFolder) == 0;
}




/**
class Settings : public settings::Settings {
public:
    class General : public settings::Section {
    public:
        settings::String target;
        General() {
            registerValue(target, "target", "Determines the path where the inputs and results of the program lie. ");
        }
    };

    class Cleaner : public settings::Command {
    public:
        settings::String inputFile;
        settings::TypedArray<std::string> languages;
        settings::Bool includeForks;

        Cleaner():
            includeForks(false) {
            registerValue(inputFile, {"inputFile", "if"}, "Path to the CSV file with potential projects to be downloaded from github in the ghtorrent format.");
            registerValue(languages, {"language", "l"}, "Project languages, as reported by github that should be considered.");
            registerValue(includeForks, {"forks"}, "If specified, forked projects will be included in the selection");
        }

    };

    class Downloader : public settings::Command {

    };


    General general;
    Cleaner cleaner;
    Downloader downloader;

    Settings() {
        registerSection(general, "general", "Generic configuration details applicable to all commands.");
        registerSection(cleaner, "clean", "Reads the GH torrent project list and extracts selected projects for further stages.");
        registerSection(downloader, "download", "Downloads the previously selected github projects into the target directory.");
    }

}; */

