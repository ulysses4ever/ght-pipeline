#include <iostream>


#include "worker.h"




namespace {

    thread_local int threadId_ = -1;

    std::mutex om_;
}


int threadId() {
    return threadId_;
}

void setThreadId(int value) {
    threadId_ = value;
}

void Write(std::string what) {
    std::lock_guard<std::mutex> g(om_);
    std::cout << what;
    if (threadId() != -1)
        std::cout << " [thread " << threadId() << "]";
    std::cout << std::endl;

}

void Error(std::string what) {
    std::lock_guard<std::mutex> g(om_);
    std::cerr << what;
    if (threadId() != -1)
        std::cerr << " [thread " << threadId() << "]";
    std::cerr << std::endl;
}

void Log(std::string what) {
    //Write(what);
}
