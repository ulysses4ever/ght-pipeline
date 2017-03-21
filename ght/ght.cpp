#include <cstdlib>
#include <iostream>
#include <thread>
#include <vector>
#include <string>

#include "settings.h"


#include "cleaner/cleaner.h"
#include "downloader/downloader.h"

#include "sovf_downloader/sovf_downloader.h"



//std::string Settings::General::Target = "/home/peta/ele2";
std::string Settings::General::Target = "/data/ele/download";
bool Settings::General::Incremental = false;
unsigned Settings::General::NumThreads = 1;
long Settings::General::DebugLimit = -1;
long Settings::General::DebugSkip = 0;
std::vector<std::string> Settings::General::ApiTokens;

unsigned Settings::General::FilesPerFolder = 1000;


void Settings::General::LoadAPITokens(std::string const & from) {
    CSVParser p(from);
    for (auto row : p)
        ApiTokens.push_back(row[0]);
}

std::string const & Settings::General::GetNextApiToken() {
    std::lock_guard<std::mutex> g(apiTokenGuard_);
    unsigned i = apiTokenIndex_++;
    if (apiTokenIndex_ >= ApiTokens.size())
        apiTokenIndex_ = 0;
    return ApiTokens[i];
}


unsigned Settings::General::apiTokenIndex_ = 0;
std::mutex Settings::General::apiTokenGuard_;




std::vector<std::string> Settings::Cleaner::InputFiles = { "/home/peta/delete/projects.csv" };
std::vector<std::string> Settings::Cleaner::AllowedLanguages = {"JavaScript"};
bool Settings::Cleaner::AllowForks = false;



std::vector<std::string> Settings::Downloader::AllowPrefix = {};
std::vector<std::string> Settings::Downloader::AllowSuffix = {".js"};
std::vector<std::string> Settings::Downloader::AllowContents = { "package.json" };
std::vector<std::string> Settings::Downloader::DenyPrefix = { "node_modles/" };
std::vector<std::string> Settings::Downloader::DenySuffix = {};
std::vector<std::string> Settings::Downloader::DenyContents = {"/node_modules/"};

bool Settings::Downloader::CompressFileContents = true;
bool Settings::Downloader::CompressInExtraThread = true;
int Settings::Downloader::MaxCompressorThreads = 4;
bool Settings::Downloader::KeepRepos = false;




void Clean() {
    Cleaner::LoadPreviousRun();
    Cleaner::Spawn(1);
    ProgressReporter::Start(Cleaner::GetReporterFeeder());
    Cleaner::Run();
    Cleaner::FeedFrom(Settings::Cleaner::InputFiles);
    Cleaner::Wait();
    Cleaner::Finalize();
}

void Download() {
    Settings::General::LoadAPITokens(STR(Settings::General::Target << "/apitokens.csv"));
    Downloader::Initialize();
    Downloader::LoadPreviousRun();
    Downloader::Spawn(Settings::General::NumThreads);
    ProgressReporter::Start(Downloader::GetReporterFeeder());
    Downloader::Run();
    Downloader::FeedFrom(Cleaner::OutputFilename());
    Downloader::Wait();
    Downloader::Finalize();
}

void DownloadStackOverflow() {
    SOvfDownloader::Initialize();
    SOvfDownloader::Download();
}



int main(int argc, char * argv[]) {
    try {
        std::cout << "OH HAI!" << std::endl;


        // Clean();
        // Download();
        DownloadStackOverflow();



        // do the reporting


        std::cout << "KTHXBYE." << std::endl;
        return EXIT_SUCCESS;
    } catch (std::exception const & e) {
        std::cout << "OH NOEZ." << std::endl;
        std::cerr << "Error: " << e.what() << std::endl;
        return EXIT_FAILURE;
    }
}
