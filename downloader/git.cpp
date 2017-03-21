#include "include/utils.h"
#include "include/exec.h"

#include "git.h"


#include <iostream>

bool Git::Clone(std::string const & url, std::string const & into) {
    std::string cmd = STR("GIT_TERMINAL_PROMPT=0 git clone " << url << " " << into);
    std::string out = execAndCapture(cmd, "");
    return (out.find("fatal:") == std::string::npos);
}

/** Returns list of all branches in the given repository.
 */
std::unordered_set<std::string> Git::GetBranches(std::string const & repoPath) {
    std::string cmd = "git branch -r";
    std::string branches = execAndCapture(cmd, repoPath);
    // now analyze the result for the branch names
    std::unordered_set<std::string> result;
    std::size_t i = 0;
    while (i < branches.size()) {
        // skip leading whitespace
        while (branches[i] == ' ')
            ++i;
        // now everything till ' ' or '\n' is branch name
        std::size_t start = i + 7;
        while (branches[i] != ' ' and branches[i] != '\n')
            ++i;
        if (branches[i] == '\n') {
            // now everything up to i is the branch
            result.insert(branches.substr(start, i - start));
            ++i;
        } else {
            // skip extra info about the branch
            while (branches[i++] != '\n') { }
        }
    }
    return result;
}


std::string Git::GetCurrentBranch(std::string const & repoPath) {
    std::string cmd = "git rev-parse --abbrev-ref HEAD";
    std::string result;
    if (execAndCapture(cmd,repoPath, result))
        return result.substr(0, result.size() - 1); // ignore the new line at the end of the output
    else
        throw std::ios_base::failure(STR("Unable to get current branch in " << repoPath));
}

std::string Git::GetLatestCommit(std::string const & repoPath) {
    std::string cmd = "git rev-parse HEAD";
    std::string result;
    if (execAndCapture(cmd,repoPath, result))
        return result.substr(0, result.size() - 1); // ignore the new line at the end of the output
    else
        throw std::ios_base::failure(STR("Unable to get latest commit in " << repoPath));
}

void Git::SetBranch(std::string const & repoPath, std::string const branch) {
    std::string cmd = STR("git checkout \"" << branch << "\"");
    std::string output; // silenc the console output of git
    if (not execAndCapture(cmd, repoPath, output))
        throw std::ios_base::failure(STR("Unable to checkout branch " << branch << " in " << repoPath));
}

Git::BranchInfo Git::GetBranchInfo(std::string const & repoPath) {
    std::string name = GetCurrentBranch(repoPath);
    std::string commit = GetLatestCommit(repoPath);
    std::string cmd = STR("git show -s --format=%at " << commit);
    std::string result;
    if (execAndCapture(cmd,repoPath, result))
        return BranchInfo(name, commit, std::stoi(result));
    else
        throw std::ios_base::failure(STR("Unable to get current branch info " << repoPath));
}


std::vector<Git::FileInfo> Git::GetFileInfo(std::string const & repoPath) {
    std::string cmd = STR("git log --format=\"format:%at\" --name-only --diff-filter=A");
    std::string files = execAndCapture(cmd, repoPath);
    // now analyze the files and their dates
    std::vector<FileInfo> result;
    std::stringstream ss(files);
    int date = 0;
    while (not ss.eof()) {
        std::string tmp;
        std::getline(ss, tmp, '\n');
        if (tmp.empty()) {
            date = 0;
        } else if (date == 0) {
            date = std::stoi(tmp);
        } else {
            result.push_back(FileInfo(tmp, date));
        }
    }
    return result;
}

// todo only works when the file exists
std::vector<Git::FileHistory> Git::GetFileHistory(std::string const & repoPath, FileInfo const & file) {
    std::string cmd = STR("git log --format=\"format:%at %H\" -- \"" << file.filename << "\"");
    //std::string cmd = STR("git log --format=\"format:%at %H\" " << filename);
    std::string history = execAndCapture(cmd, repoPath);
    std::vector<FileHistory> result;
    std::size_t i = 0;
    while (i < history.size()) {
        int date = 0;
        while (history[i] != ' ')
            date = date * 10 + (history[i++] - '0');
        //std::cout << "date " << date << std::endl;
        std::size_t start = ++i; // the space
        while (i < history.size() and history[i] != '\n')
            ++i;
        //std::cout << "start: " << start << ", end: " << i << ", : " << history.substr(start, i - start) << std::endl;
        std::string hash = history.substr(start, i - start);
		++i; // \n
        result.push_back(FileHistory(file.filename, date, hash));
    }
    return result;
}

std::string Git::GetFileRevision(std::string const & repoPath, std::string const & relPath, std::string const & commit) {
    std::string cmd = STR("git show " << commit << ":" << "\"" << relPath << "\"");
    std::string result;
    if (not execAndCapture(cmd, repoPath, result))
        throw std::ios_base::failure(STR("Unable to get file contents for file " << relPath << " commit " << commit << " at " << relPath << ", git says: " << result));
    return result;

}

void Git::Checkout(std::string const &repoPath, std::string const & commit) {
    std::string cmd = STR("git checkout " << commit);
    std::string result;
    if (not execAndCapture(cmd, repoPath, result))
        throw std::ios_base::failure(STR("Command " << cmd << " failed in " << repoPath << " with message: " << result));
}


std::vector<Git::Commit> Git::GetCommits(std::string const & repoPath) {
    std::string cmd = "git log --format=\"%H %at\"";
    std::string result;
    if (not execAndCapture(cmd, repoPath, result))
        throw std::ios_base::failure(STR("Command " << cmd << " failed in " << repoPath << " with message: " << result));
    std::vector<Commit> commits;
    std::size_t i = 0;
    while (i < result.size()) {
        std::string hash = result.substr(i, 40);
        i += 41; // for the space
        int time = 0;
        while (result[i] != '\n')
            time = (time * 10) + (result[i++] - '0');
        ++i; // new line
        commits.push_back(Commit(hash, time));
    }
    return commits;
}

std::vector<std::string> Git::GetChanges(std::string const & repoPath, std::string const & commit) {
    std::string cmd = STR("git show --oneline --name-only " << commit);
    std::string result;
    if (not execAndCapture(cmd, repoPath, result))
        throw std::ios_base::failure(STR("Command " << cmd << " failed in " << repoPath << " with message: " << result));
    std::vector<std::string> changes;
    std::size_t i = 0;
    while (i < result.size()) {
        std::size_t start = i;
        while (result[i] != '\n')
            ++i;
        ++i;
        if (start == 0) // skip the first line
            continue;
        changes.push_back(result.substr(start, i-start-1));
    }
    return changes;
}



// git log branch can be done w/o doing a branch
// git log all
