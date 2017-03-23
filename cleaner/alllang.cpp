#include "alllang.h"


long CleanerAllLang::skipped_ = 0;
long CleanerAllLang::added_ = 0;
long CleanerAllLang::total_ = 0;

std::unordered_set<std::string> CleanerAllLang::projects_;
