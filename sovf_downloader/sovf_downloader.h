#pragma once

#include <fstream>

#include "include/exec.h"
#include "include/filesystem.h"
#include "include/timer.h"


#include "ght/settings.h"

/** Stack overflow */
class SOvfDownloader {
public:
    static void Initialize() {
        createPathIfMissing(StackOvfFolder());
    }

    static void Download() {
        Timer t;
        DownloadArchive("Badges.7z");
        DownloadArchive("Comments.7z");
        DownloadArchive("PostHistory.7z");
        DownloadArchive("PostLinks.7z");
        DownloadArchive("Posts.7z");
        DownloadArchive("Tags.7z");
        DownloadArchive("Users.7z");
        DownloadArchive("Votes.7z");
        std::ofstream f = CheckedOpen(STR(StackOvfFolder() << "/runs_sovf_downloader.csv"), Settings::General::Incremental);
        f << Timer::SecondsSinceEpoch() << ","
          << t.seconds() << std::endl;
    }

private:

    static void DownloadArchive(std::string const & name) {
        std::string cmd = STR("curl https://archive.org/download/stackexchange/stackoverflow.com-"<< name << " -o " << name);
        if (not exec(cmd, StackOvfFolder()))
            throw std::runtime_error(STR("Unable to download " << name));
    }


    static std::string StackOvfFolder() {
        return STR(Settings::General::Target << "/stackovf");
    }


};
