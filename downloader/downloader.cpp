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
    // TODO
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
        // clear the last id's buffer and checkout the branch
        lastIds_.clear();
        Git::SetBranch(repoPath_, b);
        // for all commits, in reverse
        std::vector<Git::Commit> commits = Git::GetCommits(repoPath_);
        Branch branch(b, commits.back().hash);
        // append the branch to list of branches if we haven't seen it yet
        if (branches_.insert(branch).second)
            fBranches << branch << std::endl;
        for (auto i = commits.rbegin(), e = commits.rend(); i != e; ++i) {
            Commit c(*i);
            if (not commits_.insert(c).second)
                continue;
            // we haven't seen the commit yet, store it and analyze
            fCommits << c << std::endl;
            analyzeCommit(filter, *i, fSnapshots);
        }
    }
}


void Project::analyzeCommit(PatternList const & filter, Commit const & c, std::ostream & fSnapshots) {
    Git::Checkout(repoPath_, c.commit);
    // list all files changed in the commit
    for (auto f : Git::GetChanges(repoPath_,c.commit)) {
        if (not filter.check(f, hasDeniedFiles_))
            continue;
        Snapshot s(snapshots_.size(), f, c);
        // set the parent id if we have one, keep -1 if not
        auto i = lastIds_.find(s.relPath);
        if (i != lastIds_.end())
            s.parentId = i->second;
        // check if the file exists and grab it
        std::string filePath = STR(repoPath_ << "/" << f);
        if (isFile(filePath)) {
            std::string contents = LoadEntireFile(filePath);
            s.contentId = Downloader::AssignContentsId(contents);
            lastIds_[s.relPath] = s.id;
        } else {
            // deleted files have their contentId set to -1
            // and the lastIds of the relative path has to be set to -1
            lastIds_[s.relPath] = -1;
        }
        // append the snapshot to the list
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



//std::atomic<long> Project::idCounter_(0);


std::ofstream Downloader::failedProjectsFile_;

std::ofstream Downloader::contentHashesFile_;

std::unordered_map<Hash,long> Downloader::contentHashes_;

std::mutex Downloader::contentGuard_;
std::mutex Downloader::failedProjectsGuard_;
std::mutex Downloader::contentFileGuard_;

PatternList Downloader::language_;

std::atomic<long> Downloader::bytes_(0);
std::atomic<int> Downloader::compressors_(0);


#ifdef HAHA

// Project::File ----------------------------------------------------------------------------------

std::size_t Project::File::Hash::operator()(Project::File const & f ) const {
    return std::hash<std::string>()(f.commit_) + std::hash<std::string>()(f.relPath_);
}

Project::File::File(Git::FileHistory const & h, long lastId):
    isNew_(true),
    id_(-1),
    relPath_(h.filename),
    commit_(h.hash),
    time_(h.date),
    contentId_(-1),
    parentId_(lastId) {
}


Project::File::File(std::vector<std::string> const & row):
    isNew_(false) {
    if (row.size() != 6)
        throw std::ios_base::failure("Invalid row format form Project::File");
    id_ = std::atol(row[0].c_str());
    relPath_ = row[1];
    commit_ = row[2];
    time_ = std::atoi(row[3].c_str());
    contentId_ = std::atol(row[4].c_str());
    parentId_ = std::atol(row[5].c_str());
}

std::ostream & operator << (std::ostream & s, Project::File const & f) {
    s << f.id_ << ","
      << escape(f.relPath_) << ","
      << f.commit_ << ","
      << f.time_ << ","
      << f.contentId_ << ","
      << f.parentId_;
    return s;
}




// Project::Branch --------------------------------------------------------------------------------

std::size_t Project::Branch::Hash::operator()(Project::Branch const & b ) const {
    return std::hash<std::string>()(b.name_) + std::hash<std::string>()(b.commit_);
}

Project::Branch::Branch(Git::BranchInfo const & branch):
    isNew_(true),
    name_(branch.name),
    commit_(branch.commit),
    time_(branch.date) {
}

Project::Branch::Branch(std::vector<std::string> const & row):
    isNew_(false) {
    if (row.size() != 4)
        throw std::ios_base::failure("Invalid row format form Project::Branch");
    name_ = row[0];
    commit_ = row[1];
    time_ = std::atoi(row[2].c_str());
    // TODO parse the files which belong to the branch ?
}




std::ostream & operator << (std::ostream & s, Project::Branch const & f) {
    s << escape(f.name_) << ","
      << f.commit_ << ","
      << f.time_ << ",";
    std::stringstream ss;
    auto i = f.files_.begin();
    if (i != f.files_.end()) {
        ss << *i;
        ++i;
        while (i != f.files_.end()) {
            ss << "," << *i;
            ++i;
        }
    }
    s << escape(ss.str());
    return s;
}


// Project ----------------------------------------------------------------------------------------

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

void Project::initialize() {
    createPathIfMissing(path_);
}

void Project::loadPreviousRun() {
    std::string filesPath = STR(path_ << "/files.csv");
    std::string branchesPath = STR(path_ << "/branches.csv");
    // if we are missing either of the files, there is nothing to preload
    if (not isFile(filesPath) or not isFile(branchesPath))
        return;
    // load all files
    CSVParser pf(filesPath);
    for (auto row : pf)
        files_.insert(File(row));
    // load all branches
    CSVParser bf(branchesPath);
    for (auto row : bf)
        branches_.insert(Branch(row));
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

void Project::writeStats(bool append) {
    std::string filesPath = STR(path_ << "/files.csv");
    std::string branchesPath = STR(path_ << "/branches.csv");
    std::string logPath = STR(path_<< "/log.csv");
    // write files
    std::ofstream f = CheckedOpen(filesPath, append);
    for (File const & file: files_)
        if (not append or file.isNew())
            f << file << std::endl;
    // write branches
    std::ofstream b = CheckedOpen(branchesPath, append);
    for (Branch const & branch: branches_)
        if (not append or branch.isNew())
            b << branch << std::endl;
    // write the project log
    std::ofstream l = CheckedOpen(logPath, append);
    l << *this << std::endl;
}

void Project::loadFileSnapshots(PatternList const & filter) {
    for (std::string const & branch : Git::GetBranches(repoPath_)) {
        Git::SetBranch(repoPath_, branch);
        Branch b(Git::GetBranchInfo(repoPath_));
        // if we have already seen the branch at the exact commit, we can skip it
        if (branches_.find(b) != branches_.end())
            continue;
        // it is a new branch or a new commit
        for (Git::FileInfo file : Git::GetFileInfo(repoPath_)) {
            // check if we should ignore the file, or if it is a denied one
            if (not filter.check(file.filename, hasDeniedFiles_))
                continue;
            std::vector<Git::FileHistory> history = Git::GetFileHistory(repoPath_, file);
            long lastId = -1;
            for (auto i = history.rbegin(), e = history.rend(); i != e; ++i) {
                // create the file object and see if we have already seen it
                File f(*i, lastId);
                auto fi = files_.find(f);
                if (fi == files_.end()) {
                    f.id_ = files_.size();
                    // deal with the content now
                    try {
                        std::string content = Git::GetFileRevision(repoPath_, f.relPath_, f.commit_);
                        f.contentId_ = Downloader::AssignContentsId(content);
                        lastId = f.id();
                    } catch (std::ios_base::failure & e) {
                        // if there was an error, the file has been deleted
                        lastId = -1;
                    }
                    files_.insert(f);
                } else {
                    // if we have already seen the file, set lastId to its id, unless the file snapshot is file deletion
                    if (fi->contentId_ == -1)
                        lastId = -1;
                    else
                        lastId = fi->id();
                }
            }
            if (isFile(STR(repoPath_ << "/" << file.filename))) {
                if (lastId == -1)
                    throw std::runtime_error(STR("Project " << id_ << " branch " << branch << " file " << file.filename << " exists in the repo, but reported as deleted"));
                b.files_.push_back(lastId);
            }
        }
        // add the branch to list of branches captured for the project
        branches_.insert(b);
    }
}

#endif


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

    // open the streams
    // TODO should the failed projects be suffixed with run id?
    failedProjectsFile_ = CheckedOpen(STR(Settings::General::Target << "/failed_projects.csv"));
    contentHashesFile_ = CheckedOpen(STR(Settings::General::Target << "/content_hashes.csv"), Settings::General::Incremental);
}

void Downloader::LoadPreviousRun() {
    // load the file contents
    if (not Settings::General::Incremental)
        return;
    std::string content_hashes = STR(Settings::General::Target << "/content_hashes.csv");
    // TODO
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

long Downloader::AssignContentsId(std::string const & contents) {
    bytes_ += contents.size();
    Hash h = Hash::Calculate(contents);
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



