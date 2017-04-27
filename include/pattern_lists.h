#pragma once

#include <string>
#include <unordered_set>


// TODO move this to proper settings, or settings section even?

/** Contants white and black lists for file names.
 */
class PatternList {
public:

    static PatternList JavaScript() {
        PatternList js;
        js.allow("package.json"); // NPM packages information
        js.allowSuffix(".js"); // allow Javascript (including .min.js)
        js.denyContains("/node_modules/"); // allow any directory named node_modules
        js.denyPrefix("node_modules/");
        js.allowSuffix(".cpp"); // TESTING TESTING TESTING
        return js;
    }

    static PatternList Julia() {
        julia.allowSuffix(".jl");
        return js;
    }

    /** Returns true if the given filename should be analyzed, false if not.

      First the denied exact names, prefixes, suffixes and contains are checked. If the filename does not violate these, then the allowed items are checked and if at least one matches, true is returned.

      Sets denied to true if the file is blacklisted.

     */
    bool check(std::string const & filename, bool & denied) const {
        bool deny =
                checkName(deny_, filename) or
                checkSuffix(denySuffix_, filename) or
                checkPrefix(denyPrefix_, filename) or
                checkContains(denyContains_, filename);
        if (deny) {
            ++denied;
            return false;
        }
        return
            checkName(allow_, filename) or
            checkSuffix(allowSuffix_, filename) or
            checkPrefix(allowPrefix_, filename) or
            checkContains(allowContains_, filename);
    }

    void allow(std::string const & what) {
        allow_.insert(what);
    }

    void allowSuffix(std::string const & what) {
        allowSuffix_.insert(what);
    }

    void allowPrefix(std::string const & what) {
        allowPrefix_.insert(what);
    }

    void allowContains(std::string const & what) {
        allowContains_.insert(what);
    }

    void deny(std::string const & what) {
        deny_.insert(what);
    }

    void denySuffix(std::string const & what) {
        denySuffix_.insert(what);
    }

    void denyPrefix(std::string const & what) {
        denyPrefix_.insert(what);
    }

    void denyContains(std::string const & what) {
        denyContains_.insert(what);
    }

private:

    bool checkName(std::unordered_set<std::string> const & x, std::string const & what) const {
        return x.find(what) != x.end();
    }

    bool checkSuffix(std::unordered_set<std::string> const & x, std::string const & what) const {
        for (std::string const & xx : x) {
            int y = what.size() - xx.size();
            if (y >= 0)
                if (what.find(xx, y) == y)
                    return true;
        }
        return false;
    }

    bool checkPrefix(std::unordered_set<std::string> const & x, std::string const & what) const {
        for (std::string const & xx : x)
            if (what.find(xx) == 0)
                return true;
        return false;
    }

    bool checkContains(std::unordered_set<std::string> const & x, std::string const & what) const {
        for (std::string const & xx : x) {
            auto i = what.find(xx);
            if (i < what.size() and i != std::string::npos)
                return true;
        }
        return false;
    }


    std::unordered_set<std::string> allow_;
    std::unordered_set<std::string> allowSuffix_;
    std::unordered_set<std::string> allowPrefix_;
    std::unordered_set<std::string> allowContains_;
    std::unordered_set<std::string> deny_;
    std::unordered_set<std::string> denySuffix_;
    std::unordered_set<std::string> denyPrefix_;
    std::unordered_set<std::string> denyContains_;
};
