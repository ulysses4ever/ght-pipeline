#include "downloader.h"

#include "include/csv.h"
#include "include/exec.h"


#include "git.h"

std::atomic<long> Project::idCounter_(0);


std::atomic<long> Downloader::snapshots_(0);



void Project::initialize() {
    createPathIfMissing(path_);

}

void Project::loadPreviousRun() {
    // TODO actually load the previous run, for now, we just
}

void Project::clone(bool force) {
    if (isDirectory(repoPath_)) {
        if (force) {
            deletePath(repoPath_);
        } else {
            // TODO pull instead of clone
            return;
        }
    }
    if (not Git::Clone(gitUrl(), repoPath_))
        throw std::runtime_error(STR("Unable to download project " << gitUrl() << ", id " << id_));

}

void Project::loadMetadata() {
    // no tokens, no metadata
    if (Settings::General::ApiTokens.empty())
        return;
    // get the current API token (remember we are rotating them)
    std::string const & token = Settings::General::GetNextApiToken();
    // construct the API request
    std::string req = STR("curl -i -s " << apiUrl() << " -H \"Authorization: token " << token << "\"");
    // exec, the captured string is the result
    req = execAndCapture(req, path_);
    // TODO analyze the results somehow

    std::ofstream m = CheckedOpen(STR(path_ << "/metadata.json"));
    m << req;
}

void Project::deleteRepo() {
    deletePath(repoPath_);
}

void Project::finalize() {
    std::ofstream fLog = CheckedOpen(fileLog(), Settings::General::Incremental);
    fLog << *this << std::endl;
}

void Project::analyze(PatternList const & filter) {
    // open the output streams
    std::ofstream fBranches = CheckedOpen(fileBranches(), Settings::General::Incremental);
    std::ofstream fCommits = CheckedOpen(fileCommits(), Settings::General::Incremental);
    std::ofstream fSnapshots = CheckedOpen(fileSnapshots(), Settings::General::Incremental);
    // for all branches
    for (std::string const & b: Git::GetBranches(repoPath_)) {
        // clear the last id's buffer
        lastIds_.clear();
        // get all commits for the branch
        std::vector<Git::Commit> commits = Git::GetCommits(repoPath_,b);
        Branch branch(b, commits.back().hash);
        // append the branch to list of branches if we haven't seen it yet
        if (branches_.insert(branch).second)
            fBranches << branch << std::endl;
        std::string parent = "";
        for (auto i = commits.rbegin(), e = commits.rend(); i != e; ++i) {
            Commit c(*i);
            if (not commits_.insert(c).second)
                continue;
            // we haven't seen the commit yet, store it and analyze
            fCommits << c << std::endl;
            analyzeCommit(filter, *i, parent, fSnapshots);
            parent = c.commit;
        }
    }
}

void Project::analyzeCommit(PatternList const & filter, Commit const & c, std::string const & parent, std::ostream & fSnapshots) {
    bool checked = false;
    for (auto obj : Git::GetObjects(repoPath_, c.commit, parent)) {
        // check if it is a language file
        if (not filter.check(obj.relPath, hasDeniedFiles_))
            continue;
        Snapshot s(snapshots_.size(), obj.relPath, c);
        // set the parent id if we have one, keep -1 if not
        auto i = lastIds_.find(s.relPath);
        if (i != lastIds_.end())
            s.parentId = i->second;
        if (obj.type == Git::Object::Type::Deleted) {
            s.contentId = -1;
            lastIds_[s.relPath] = -1;
        } else {
            if (not checked) {
                Git::Checkout(repoPath_, c.commit);
                checked = true;
            }
            s.contentId = Downloader::AssignContentId(SHA1(obj.hash), obj.relPath, repoPath_);
            lastIds_[s.relPath] = s.id;
        }
        snapshots_.push_back(s);
        fSnapshots << s << std::endl;
        ++Downloader::snapshots_;
    }
}

Project::Project(std::string const & relativeUrl):
    id_(idCounter_++),
    url_(relativeUrl) {
    path_ = STR(Settings::General::Target << "/projects" << IdToPath(id_, "projects_") << "/" << id_);
    repoPath_ = STR(path_ << "/repo");
}

Project::Project(std::string const & relativeUrl, long id):
    id_(id),
    url_(relativeUrl) {
    ++id;
    while (idCounter_ < id) {
        long old = idCounter_;
        if (idCounter_.compare_exchange_weak(old, id))
            break;
    }
    path_ = STR(Settings::General::Target << "/projects" << IdToPath(id_, "projects_") << "/" << id_);
    repoPath_ = STR(path_ << "/repo");
}


std::ofstream Downloader::failedProjectsFile_;

std::ofstream Downloader::contentHashesFile_;

std::unordered_map<SHA1,long> Downloader::contentHashes_;

std::mutex Downloader::contentGuard_;
std::mutex Downloader::failedProjectsGuard_;
std::mutex Downloader::contentFileGuard_;

PatternList Downloader::language_;

std::atomic<long> Downloader::bytes_(0);
std::atomic<int> Downloader::compressors_(0);


// Downloader -------------------------------------------------------------------------------------

void Downloader::Initialize() {
    // fill in the language filter object
    for (auto i : Settings::Downloader::AllowPrefix)
        language_.allowPrefix(i);
    for (auto i : Settings::Downloader::AllowSuffix)
        language_.allowSuffix(i);
    for (auto i : Settings::Downloader::AllowContents)
        language_.allow(i);
    for (auto i : Settings::Downloader::DenyPrefix)
        language_.denyPrefix(i);
    for (auto i : Settings::Downloader::DenySuffix)
        language_.denySuffix(i);
    for (auto i : Settings::Downloader::DenyContents)
        language_.deny(i);

}

void Downloader::LoadPreviousRun() {
    // load the file contents
    if (not Settings::General::Incremental)
        return;
    std::string content_hashes = STR(Settings::General::Target << "/content_hashes.csv");
    std::cout << "Loading content hashes from previous runs" << std::flush;

    CSVParser p(content_hashes);
    for (auto i : p) {
        SHA1 hash(i[0]);
        long id = std::stol(i[1]);
        contentHashes_.insert(std::make_pair(hash, id));
        if (contentHashes_.size() % 10000000 == 0)
            std::cout << "." << std::flush;
    }
    std::cout << std::endl << "    " << contentHashes_.size() << " ids loaded" << std::endl;
}

void Downloader::OpenOutputFiles() {
    // open the streams
    // TODO should the failed projects be suffixed with run id?
    failedProjectsFile_ = CheckedOpen(STR(Settings::General::Target << "/failed_projects.csv"));
    contentHashesFile_ = CheckedOpen(STR(Settings::General::Target << "/content_hashes.csv"), Settings::General::Incremental);
}

void Downloader::FeedFrom(std::string const & filename) {
    CSVParser p(filename);
    long line = 1;
    long i = 0;
    for (auto x : p) {
        if (Settings::General::DebugSkip > 0) {
            --Settings::General::DebugSkip;
            ++line;
            continue;
        }
        ++line;
        if (Settings::General::DebugLimit != -1 and i >= Settings::General::DebugLimit)
            break;
        ++i;
        if (x.size() == 1) {
            Project p(x[0]);
            if (not isFile(p.fileLog()))
                Schedule(Project(x[0]));
            continue;
        } else if (x.size() == 2) {
            try {
                char ** c;
                Schedule(Project(x[0], std::strtol(x[1].c_str(), c, 10)));
                continue;
            } catch (...) {
                // the code below outputs the error too
            }
        }
        Error(STR(filename << ", line " << line << ": Invalid format of the project url input, skipping."));
    }
}

void Downloader::Finalize() {
    failedProjectsFile_.close();
    contentHashesFile_.close();
    std::ofstream stamp = CheckedOpen(STR(Settings::General::Target << "/runs_downloader.csv"), Settings::General::Incremental);
    stamp << Timer::SecondsSinceEpoch() << ","
          << Project::idCounter_ << ","
          << ErrorTasks() << ","
          << contentHashes_.size() << ","
          << snapshots_ << ","
          << TotalTime() << std::endl;
    // a silly busy wait
    while (compressors_ > 0) {
    }
}


ProgressReporter::Feeder Downloader::GetReporterFeeder() {
    return [](ProgressReporter & p, std::ostream & s) {
        p.done = CompletedTasks();
        p.errors = ErrorTasks();
        if (AllDone())
            p.allDone = true;
        int i = 0;
        int j = 0;
        for (Downloader * d : threads_) {
            if (d != nullptr) {
                s << "[" << std::right << std::setw(2) << i << "]" << std::left << std::setw(16) << d->status() << " ";
                if (++j == 4) {
                    j = 0;
                    s << std::endl;
                }
            }
            ++i;
        }
        if (j != 0)
            s << std::endl;
        s << "total bytes: " << std::setw(10) << std::left << Bytes(bytes_);
        s << "unique files: " << std::setw(16) << std::left << contentHashes_.size();
        s << "snapshots: " << std::setw(16) << std::left << snapshots_;
        s << "active compressors " << compressors_ << std::endl;
    };
}

long Downloader::AssignContentId(SHA1 const & hash, std::string const & relPath, std::string const & root) {
    long id;
    {
        std::lock_guard<std::mutex> g(contentGuard_);
        auto i = contentHashes_.find(hash);
        if (i != contentHashes_.end())
            return i->second;
        id = contentHashes_.size();
        contentHashes_.insert(std::make_pair(hash, id));
    }
    std::string contents = LoadEntireFile(STR(root << "/" << relPath));
    bytes_ += contents.size();
    // we have a new hash now, the file contents must be stored and the contents hash file appended
    std::string targetDir = STR(Settings::General::Target << "/files" << IdToPath(id, "files_"));
    createPathIfMissing(targetDir);
    {
        std::ofstream f = CheckedOpen(STR(targetDir << "/" << id << ".raw"));
        f << contents;
    }
    // output the mapping
    {
        std::lock_guard<std::mutex> g(contentFileGuard_);
        contentHashesFile_ << hash << "," << id << std::endl;
    }
    // compress the folder if it is full
    if (Settings::Downloader::CompressFileContents and IdPathFull(id)) {
        if (Settings::Downloader::CompressInExtraThread and compressors_ < Settings::Downloader::MaxCompressorThreads) {
            std::thread t([targetDir] () {
                CompressFiles(targetDir);
            });
            t.detach();
        } else {
            CompressFiles(targetDir);
        }
    }
    return id;
}






long Downloader::AssignContentsId(std::string const & contents) {
    bytes_ += contents.size();
    SHA1 h;
    //Hash h = Hash::Calculate(contents);
    long id;
    {
        std::lock_guard<std::mutex> g(contentGuard_);
        auto i = contentHashes_.find(h);
        if (i != contentHashes_.end())
            return i->second;
        id = contentHashes_.size();
        contentHashes_.insert(std::make_pair(h, id));
    }
    // we have a new hash now, the file contents must be stored and the contents hash file appended
    std::string targetDir = STR(Settings::General::Target << "/files" << IdToPath(id, "files_"));
    createPathIfMissing(targetDir);
    {
        std::ofstream f = CheckedOpen(STR(targetDir << "/" << id << ".raw"));
        f << contents;
    }
    // output the mapping
    {
        std::lock_guard<std::mutex> g(contentFileGuard_);
        contentHashesFile_ << h << "," << id << std::endl;
    }
    // compress the folder if it is full
    if (Settings::Downloader::CompressFileContents and IdPathFull(id)) {
        if (Settings::Downloader::CompressInExtraThread and compressors_ < Settings::Downloader::MaxCompressorThreads) {
            std::thread t([targetDir] () {
                CompressFiles(targetDir);
            });
            t.detach();
        } else {
            CompressFiles(targetDir);
        }
    }
    return id;
}

void Downloader::CompressFiles(std::string const & targetDir) {
    try {
        ++compressors_;
        bool ok = true;
        ok = exec("tar cfJ files.tar.xz *.raw", targetDir);
        ok = exec("rm -f *.raw", targetDir);
        if (not ok)
            std::cerr << "Unable to compress files in " << targetDir << std::endl;
    } catch (...) {
    }
    --compressors_;
}



