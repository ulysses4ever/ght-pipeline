/**

  Downloader now only downloads the JS files and stores them on disk. Does not attempt to tokenize them as 3rd party tokenizer will be used in the future to address parsing correctness.

  All pipeline should work w/o database and should also check histories of files. Could be easily-ish retargettable for different languages.

 */

#include <cstdlib>
#include <iostream>
#include <unordered_map>
#include <cassert>

#include "include/csv.h"
#include "include/filesystem.h"
#include "include/settings.h"
#include "include/exec.h"
#include "include/hash.h"
#include "include/pattern_lists.h"

#include "git.h"


#include "include/worker.h"



static_assert(sizeof(long) == 8, "We need a lot of ids");

class Project {
public:

    /** Creates new project pointing to the given git url.

      The project is assigned a new unique id.
     */
    Project(std::string const & gitUrl):
        id_(idIndex_++),
        gitUrl_(gitUrl) {
    }

    /** Creates a project with given url and sets its id to the provided value.

      Also updates the internal id counter so that newly created objects will have id greater than this one.
     */
    Project(std::string const & gitUrl, long id):
        id_(id),
        gitUrl_(gitUrl) {
        // make sure we update the counter, since there should be only one thread creating projects, I expect no contention and this is just defensive
        while (idIndex_ <= id) {
            long old = idIndex_;
            if (idIndex_.compare_exchange_weak(old, id + 1))
                break;
        }
    }

    /** Returns the id of the project.
     */
    long id() const {
        return id_;
    }

    /** Returns the git url of the project.
     */
    std::string const & gitUrl() const {
        return gitUrl_;
    }

    /** Returns the project url to be used when querying github API.
     */
    std::string apiUrl() const {
        std::string result = gitUrl_.substr(18, gitUrl_.size() - 22); // remove gh http and .git
        return STR("https://api.github.com/repos/" << result);
    }

    /** Default constructor required by the worker framework.
     */
    Project():
        id_(-1) {
    }

    bool & hasDeniedFiles() {
        return hasDeniedFiles_;
    }

    bool hasDeniedFiles() const {
        return hasDeniedFiles_;
    }

    bool operator == (Project const & other) const {
        return gitUrl_ == other.gitUrl_;
    }

    void writeTo(std::ostream & s) {
        s << id_ << ","
          << escape(gitUrl_) << ","
          << (hasDeniedFiles_ ? 1 : 0) << std::endl;
    }

private:
    friend std::ostream & operator << (std::ostream & s, Project const & task) {
        s << task.gitUrl() << " [" << task.id() << "]";
        return s;
    }


    /** Id of the project.
     */
    long id_;


    /** Git url from which the repo can be cloned.
     */
    std::string gitUrl_;

    /** True if there were some files that are explicitly denied.
     */
    bool hasDeniedFiles_ = false;


    static std::atomic<long> idIndex_;
};

namespace std {

    template<>
    struct hash<::Project> {

        std::size_t operator()(::Project const & p) const {
            return std::hash<std::string>{}(p.gitUrl());
        }

    };

}



/**  File Snapshots.

  The downloader itself does not work with files, but file snapshots. Each file snapshot is identified by the following:

  - commit hash string
  - relative path of the file in the given commit

  Each snapshot has its own id number and and contains an index of its previous snapshot, as well as and index of the contents of the file at that particular commit.

 */

class FileSnapshot {
public:

    std::string const & commit() const {
        return commit_;
    }

    std::string const & relPath() const {
        return relPath_;
    }

    /** Creates a file snapshot from given git file history record.
     */
    FileSnapshot(Git::FileHistory const & h):
        id_(-1),
        commit_(h.hash),
        relPath_(h.filename),
        contentId_(-1),
        time_(h.date) {
    }

    long id() const {
        return id_;
    }

    long & id() {
        return id_;
    }

    long contentId() const {
        return contentId_;
    }

    long & contentId() {
        return contentId_;
    }

    long parentId() const {
        return parentId_;
    }

    long & parentId() {
        return parentId_;
    }

    bool operator == (FileSnapshot const & other) const {
        return commit_ == other.commit_ and relPath_ == other.relPath_;
    }

    void writeTo(std::ostream & s) const {
        s << id_ << ","
          << escape(relPath_) << ","
          << commit_ << ","
          << time_ << ","
          << contentId_ << ","
          << parentId_ << std::endl;
    }

private:
    long id_;
    long parentId_;
    std::string commit_;
    std::string relPath_;
    long contentId_;
    int time_;
};

namespace std {

    template<>
    struct hash<::FileSnapshot> {

        std::size_t operator()(::FileSnapshot const & f) const {
            return std::hash<std::string>{}(f.commit()) + std::hash<std::string>{}(f.relPath());
        }

    };

}

/** Branch Snapshot

  When the project is downloaded, a branch snapshot contains the file snapshot ids for all relevant files present in the branch at the given commit. This can be used to reconstruct which file snapshots are present in the branch at the given time.

 */
class BranchSnapshot {
public:



    /** Adds given file to the branch snapshot.
     */
    void addFile(long snapshotId) {
        files_.push_back(snapshotId);
    }

    std::string const & name() const {
        return name_;
    }

    std::string const & commit() const {
        return commit_;
    }

    int date() const {
        return date_;
    }

    /** Writes the branch snapshot into the project folder.
     */
    void writeTo(std::ostream & s) const {
        s << escape(name_) << ","
          << commit_ << ","
          << date_;
        for (long f: files_)
            s << "," << f;
        s << std::endl;
    }

    BranchSnapshot(Git::BranchInfo const & b):
        name_(b.name),
        commit_(b.commit),
        date_(b.date) {
    }

    bool operator == (BranchSnapshot const & other) const {
        return name_ == other.name_ and commit_ == other.commit_;
    }

private:
    // branch name
    std::string name_;
    // commit hash at which the branch has been sampled
    std::string commit_;
    // creation time of the commit at which the snapshot has been taken
    int date_;
    // file snapshot id's that are part of the branch
    std::vector<long> files_;
};

namespace std {

    template<>
    struct hash<::BranchSnapshot> {

        std::size_t operator()(::BranchSnapshot const & b) const {
            return std::hash<std::string>{}(b.commit());
        }

    };

}






/** Downloads git projects and their files.

  The downloader can either append new projects to existing output, or it can rescan projects it has already seen for any changes and only update these. All outputs from the downloader are stored in the OutputPath and have the following structure:

  temp - this is where the temporarily downloaded projects live, the temp directory is always deleted when downloader finishes

  projects - contains scanned projects (see below)

  data - contains unique file contents (see below)

  stats - contains statistics about the downloader session (see below)

  ### Projects

  To avoid straining the filesystem, project ID's are stored hierarchically in









 */
class Downloader : public Worker<Downloader, Project> {
public:

    static void Initialize(PatternList const & p) {

        filePattern_ = p;

        // for now, just make sure the directories exist
        createPathIfMissing(Settings::OutputPath);
        createPathIfMissing(StatsPath());
        createPathIfMissing(ProjectsPath());
        createPathIfMissing(FilesPath());
    }

    static void Finalize() {
        WriteProjects();
        // output the file contents mapping
        WriteContentsMapping();

		// Write the list of failed projects
		WriteFailedProjects();

        // output the statistics
        WriteStatistics();

    }



    /** Reads the given file, and schedules each project in it for the download.

      The file should contain a git url per line.
     */
    static void FeedProjectsFrom(std::string const & filename) {
        CSVParser p(filename);
        long line = -1;
        for (auto x : p) {
            ++line;
            // skip the initial projects if requested
            if (line < Debug_FirstProjectOffset)
                continue;
            // and only do the given number of them if required
            if (Debug_MaxProjects > -1 and line > Debug_FirstProjectOffset + Debug_MaxProjects)
                break;
            if (x.size() == 1) {
                Schedule(Project(x[0]));
                continue;
            } else if (x.size() == 2) {
                try {
                    char ** c;
                    Schedule(Project(x[0], std::strtol(x[1].c_str(), c, 10)));
                    continue;
                } catch (...) {
                }
            }
            Error(STR(filename << ", line " << line << ": Invalid format of the project url input, skipping."));
        }
    }

    /** If true, once an output directory contains its maximal number of files, the files will be compressed to an archive.

      Initial findings show this reduces the output size tenfold.
     */
    static bool CompressFileContents;

    /** If true, once a github repository analysis is finished, the repository will be removed from disk.

      TODO when projects are kept, we want to store them hierarchically.
     */
    static bool DeleteClonedProjects;

    /** When true, if a project seems to be already cloned in the destination directory,

      Note that this should only be useful in debug mode.
     */
    static bool Debug_ReuseClonedProjects;

    /** Maximal number of projects to be fetched.

       All projects are fetched if -1.
     */
    static long Debug_MaxProjects;

    /** Number of projects to skip when fetching.

      If 0, no projects are skipped.
     */
    static long Debug_FirstProjectOffset;

private:

    static void WriteProjects() {
        std::ofstream f(STR(StatsPath() << "/projects.csv"));
        for (auto p : projects_)
            p.writeTo(f);
    }

    static void WriteContentsMapping() {
        std::ofstream f(STR(StatsPath() << "/contents.csv"));
        for (auto i : fileHashes_)
            f << i.first << "," << i.second << std::endl;
    }

	static void WriteFailedProjects() {
		std::ofstream f(STR(StatsPath() << "/failedProjects.csv"));
        for (auto i : failedProjects_)
            f << i.gitUrl() << "," << i.id();
	}


    /** Writes statistics about the run.

      - date of the run
      - # of projects (tasks) analyzed
      - # of project stored
      - # of projects which failed to download
      - # number of unique file contents analyzed

     */
    static void WriteStatistics() {
        std::ofstream f(STR(StatsPath() << "/runs.csv"));
        f << timestamp() << ","
          << CompletedTasks() << ","
          << projects_.size() << ","
          << ErrorTasks() << ","
          << fileHashes_.size() << std::endl;
    }

    /** For each project, the dowloader does the following:

      - attempt to clone the project, if this fails, the project's url is reported to the failed files

     */
    void run(Project & task) override {
        files_.clear();
        branches_.clear();

        Log(STR("Processing task " << task));
        // clone the project
        download(task);
        // process all branches
        processAllBranches(task);
        // store file snapshots
        writeProjectStatistics(task);
        // load the metadata
        downloadMetadata(task);
        // all work is done, delete the project
        deleteProject(task);
    }



    void download(Project & p) {
        localPath_ = STR(ProjectsPath() << Settings::IdToPath(p.id()) << "/project_" << p.id());
        createPathIfMissing(localPath_);
        localRepo_ = STR(localPath_ << "/repo");
        // if by chance the dir already exists (from last execution, remove it so that we can clone into)
        if (isDirectory(localRepo_)) {
            // unless we are reusing cloned projects
            if (Debug_ReuseClonedProjects)
                return;
            deletePath(localRepo_);
        }
        if (not Git::Clone(p.gitUrl(), localRepo_)) {
			// the project can't be downloaded, output it to the failed list
			{
				std::lock_guard<std::mutex> g(projectsGuard_);
                failedProjects_.insert(p);
			}
            throw std::runtime_error(STR("Unable to download project " << p.gitUrl()));
        }
        Log(STR(p << " successfully cloned to local path " << localRepo_));
    }

    void processAllBranches(Project & p) {
        std::unordered_set<std::string> branches = Git::GetBranches(localRepo_);
        std::string current = Git::GetCurrentBranch(localRepo_);
        while (true) {
            branches.erase(current); // remove current branch
            Log(STR("Analyzing branch " << current));
            // process all files we can find in the branch
            processFiles(p, current);
            // move to the next branch
            while (true) {
                if (branches.empty())
                    return; // we are done
                // otherwise get new branch, remove from the list
                current = * branches.begin();
                branches.erase(branches.begin());
                // and checkout
                if (not Git::SetBranch(localRepo_, current)) {
                    Error(STR("Unable to checkout branch " << current));
                    continue;
                }
                break;
            }
        }
    }

    /** Processes files in the current branch.

     */
    void processFiles(Project & p, std::string const & branchName) {
        // get the branch snapshot
        BranchSnapshot branch(Git::GetBranchInfo(localRepo_));
        // for incremental downloads, if we have already seen the branch at given commit, there is no need to parse further
        if (branches_.find(branch) != branches_.end())
            return;
        // get all files reported in the branch
        for (Git::FileInfo const & file : Git::GetFileInfo(localRepo_)) {
            bool denied = false;
            if (filePattern_.check(file.filename, denied)) {
                // get the file history and create the snapshots where missing
                std::vector<Git::FileHistory> fh = Git::GetFileHistory(localRepo_, file);
                // this is the previous revision id, -1 means no previous revision exists
                long lastId = -1;
                // iterate from last to first
                for (auto i = fh.rbegin(), e = fh.rend(); i != e; ++i) {
                    FileSnapshot fs(*i);
                    auto si = files_.find(fs);
                    if (si == files_.end()) {
                        std::string text;
                        if (Git::GetFileRevision(localRepo_, *i, text)) {
                            // assign the snapshot id
                            fs.id() = files_.size();
                            fs.parentId() = lastId;
                            fs.contentId() = getContentId(text);
                            // add the file snapshot to current project's snapshots
                            files_.insert(fs);
                            lastId = fs.id();
                        } else {
                            // if the file can't be obtained, it has been deleted. therefore set lastId to -1
                            lastId = -1;
                        }
                    } else {
                        lastId = si->id();
                    }
                }
                // if the file exists, add it to the branch information
                if (isFile(STR(localRepo_ << "/" << file.filename))) {
                    if (lastId == -1) {
                        Error(STR("PROJECT: " << p.id()));
                        Error(STR("BRANCH:  " << branchName));
                        Error(STR("FILE:    " << file.filename));
                        throw std::ios_base::failure(STR("Deleted file should not be in branch"));
                    }
                    branch.addFile(lastId);
                }
            } else if (denied) {
                // if the file has been denied... ouch
                p.hasDeniedFiles() = true;
            }
        }
        branches_.insert(branch);
    }

    long getContentId(std::string const & text) {
        // hash the file contents
        Hash h = Hash::Calculate(text);
        long id;
        {
            // lock on the file contents
            std::lock_guard<std::mutex> g(contentsGuard_);
            auto i = fileHashes_.find(h);
            if (i != fileHashes_.end())
                return i->second;
            id = fileHashes_.size();
            fileHashes_.insert(std::pair<Hash, long>(h, id));
        }
        // now we need to store the file
        std::string targetDir = STR(FilesPath() << Settings::IdToPath(id));
        createPathIfMissing(targetDir);
        std::ofstream out(STR(targetDir << "/" << id << ".raw"));
        out << text;
        out.close();
        if (CompressFileContents and Settings::ClosesPathDir(id)) {
            bool ok = true;
            ok = exec("tar cfJ files.tar.xz *.raw", targetDir);
            ok = exec("rm -f *.raw", targetDir);
            if (not ok)
                throw std::ios_base::failure(STR("Unable to compress files in " << targetDir));
        }
        return id;
    }

    void writeProjectStatistics(Project & p) {
        // create file snapshots
        std::ofstream fs(STR(localPath_ << "/files.csv"));
        for (auto const & s : files_)
            s.writeTo(fs);
        fs.close();
        // write branches
        std::ofstream bs(STR(localPath_ << "/branches.csv"));
        for (auto const & s : branches_)
            s.writeTo(bs);
        bs.close();
        // add the task to projects
        std::lock_guard<std::mutex> g(projectsGuard_);
        projects_.insert(p);
    }

    void downloadMetadata(Project & p) {
        // if there are no API tokens imported, do not download the metadata
        if (apiTokens_.empty())
            return;
        // get the API index and rotate it
        size_t tokenIndex;
        {
            std::lock_guard<std::mutex> g(apiTokensGuard_);
            tokenIndex = currentApiToken_++;
            if (currentApiToken_ == apiTokens_.size())
                currentApiToken_ = 0;
        }
        // construct the url and the request
        std::string req = STR("curl -i -s " << p.apiUrl() << " -H \"Authorization: token " << apiTokens_[tokenIndex] << "\"");
        req = execAndCapture(req, localPath_);
        // TODO analyze the results somehow to be a bit more robust

        // store the raw json data
        std::ofstream m(STR(localPath_ << "/metadata.json"));
        m << req;
    }


    /** Just deletes the local path associated with the project.
     */
    void deleteProject(Project & p) {
        if (DeleteClonedProjects) {
            deletePath(localRepo_);
            Log(STR(localRepo_ << " deleted."));
        }
    }

    static std::string StatsPath() {
        return Settings::OutputPath + "/stats";
    }

    static std::string ProjectsPath() {
        return Settings::OutputPath + "/projects";
    }

    static std::string FilesPath() {
        return Settings::OutputPath + "/files";
    }






	/** Contains the local path where the current project is stored. 
	 */
	std::string localPath_;
    std::string localRepo_;

    /** File snapshots in the current project.
     */
    std::unordered_set<FileSnapshot> files_;

    /** Branches captured in the current project.
     */
    std::unordered_set<BranchSnapshot> branches_;


    /** Contains a map of all file hashes seen so far and their ids.
     */
    static std::unordered_map<Hash, long> fileHashes_;

    /** File patterns to accept, or deny.
     */
    static PatternList filePattern_;

    /** File to which the analyzed projects are stored.
     */
    static std::unordered_set<Project> projects_;

	/** Vector which contains a list of failed projects so that they can be reattempted in the future. 
	 */
    static std::unordered_set<Project> failedProjects_;

    /** List of API tokens which the metadata downloader rotates to circumvent the single token rate limitation.
     */
public:
    static std::vector<std::string> apiTokens_;

private:

    /** Current API token to be used.
     */
    static int currentApiToken_;

    static std::mutex projectsGuard_;
    static std::mutex contentsGuard_;
    static std::mutex apiTokensGuard_;
};


bool Downloader::CompressFileContents = true;

bool Downloader::DeleteClonedProjects = true;

bool Downloader::Debug_ReuseClonedProjects = false;

long Downloader::Debug_MaxProjects = -1;

long Downloader::Debug_FirstProjectOffset = 0;




std::vector<std::string> Downloader::apiTokens_;
int Downloader::currentApiToken_ = 0;

std::mutex Downloader::apiTokensGuard_;










std::unordered_map<Hash, long> Downloader::fileHashes_;

PatternList Downloader::filePattern_;

std::unordered_set<Project> Downloader::projects_;
std::unordered_set<Project> Downloader::failedProjects_;


std::mutex Downloader::projectsGuard_;
std::mutex Downloader::contentsGuard_;












std::atomic<long> Project::idIndex_(0);








/** Things to do:

  - settings in one file that is part of the target zone
  - single executable with arguments
  - incremental
  - metadata - for metadata we need either a different downloader, or more authentication tokens
  - compressing files (DONE)

 */







int main(int argc, char * argv[]) {
    CSVParser p("/data/ele/apitokens.csv");
    for (auto row : p) {
        Downloader::apiTokens_.push_back(row[0]);
    }
    Settings::OutputPath = "/data/ele";
	//Settings::OutputPath = "/home/peta/ele";
    Downloader::Initialize(PatternList::JavaScript());
    Downloader::Spawn(16);
    Downloader::Run();
    //Downloader::FeedProjectsFrom("/home/peta/devel/ele-pipeline/project_urls.csv");
    Downloader::FeedProjectsFrom("/data/ele/projects.csv");
	//Downloader::FeedProjectsFrom("/home/peta/ele/projects.csv");
    Downloader::Wait();
    Downloader::Finalize();
    std::cout << "DONE" << std::endl;
    /*

    Downloader::Spawn(10);
    Downloader::Run();
    Downloader::Stop();


    std::cout << "Oh Hai!" << std::endl;

    std::string repoUrl = "/home/peta/devel/js-tokenizer";

    auto i = Git::getFileInfo(repoUrl);
    for (auto x : i) {
        std::cout << x.filename << " " << x.created << std::endl;
        //if (isFile(STR(repoUrl + "/" + x.filename))) {
            auto hist = Git::getFileHistory(repoUrl, x);
            for (auto hx : hist) {
                std::cout << "    " << hx.hash << " " << hx.date << " - " << hx.filename << ": ";
                std::string contents;
                if (Git::getFileRevision(repoUrl, hx, contents)) {
                    std::cout << contents.size() << std::endl;
                    //std::cout << contents;
                }
                else
                    std::cout << "DELETED";
                std::cout << std::endl;
            }
        //}
    } */

/*
    CSVParser p("/data/ghtorrent/projects.csv");
    unsigned x = 0;
    for (auto row : p) {
        for (std::string const & s : row) {
            std::cout << s << " ";
        }
        std::cout << std::endl;
        ++x;
    }
    std::cout << x << std::endl;

    */

    return EXIT_SUCCESS;
}

