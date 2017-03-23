#include "cleaner.h"


long Cleaner::skipped_ = 0;
long Cleaner::added_ = 0;
long Cleaner::total_ = 0;

std::unordered_set<std::string> Cleaner::projects_;
