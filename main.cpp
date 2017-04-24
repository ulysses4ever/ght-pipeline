#include <cstdlib>
#include <iostream>
#include <thread>
#include <vector>
#include <string>

#include "ght/settings.h"


#include "cleaner/cleaner.h"
#include "cleaner/alllang.h"
#include "downloader/downloader.h"

#include "sovf_downloader/sovf_downloader.h"


#include "stridemerger/stridemerger.h"
#include "sccsorter/sccsorter.h"


std::string Settings::General::Target = "/data/ele/download";
bool Settings::General::Incremental = true;
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


std::string Settings::CleanerAllLang::OutputFile = "/home/peta/delete/cleaned_projects.csv";


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


//std::string Settings::StrideMerger::Folder = "/data/ecoop17/datasets/js_github_all";
std::string Settings::StrideMerger::Folder = "/home/peta/strides";
void Clean() {
    Cleaner::LoadPreviousRun();
    Cleaner::Spawn(1);
    ProgressReporter::Start(Cleaner::GetReporterFeeder());
    Cleaner::Run();
    Cleaner::FeedFrom(Settings::Cleaner::InputFiles);
    Cleaner::Wait();
    Cleaner::Finalize();
}

void CleanAllLang() {
    CleanerAllLang::Spawn(1);
    ProgressReporter::Start(CleanerAllLang::GetReporterFeeder());
    CleanerAllLang::Run();
    CleanerAllLang::FeedFrom(Settings::Cleaner::InputFiles);
    CleanerAllLang::Wait();
}

void Download() {
    Settings::General::LoadAPITokens(STR(Settings::General::Target << "/apitokens.csv"));
    Downloader::Initialize();
    Downloader::LoadPreviousRun();
    Downloader::OpenOutputFiles();
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
        Download();



        /*if (argc != 4)
            throw std::runtime_error("Expected tokenized file to sort!");
        int start = std::atoi(argv[1]);
        int end = std::atoi(argv[2]);
        Settings::StrideMerger::Folder = argv[3];
        StrideMerger::Merge(start, end); */


        // Clean();
        //CleanAllLang();
        //Download();
        //DownloadStackOverflow();
        //SccSorter::Verify("/home/peta/delete/tokenized_files_0.txt");
        //StrideMerger::Merge("0-1", "2", "0-2");
        // do the reporting


        std::cout << "KTHXBYE." << std::endl;
        return EXIT_SUCCESS;
    } catch (std::exception const & e) {
        std::cout << "OH NOEZ." << std::endl;
        std::cerr << "Error: " << e.what() << std::endl;
        return EXIT_FAILURE;
    }
}
