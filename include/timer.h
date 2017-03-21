#pragma once
#include <functional>
#include <iomanip>
#include <thread>
#include <ostream>
#include <sstream>
#include <iostream>
#include <chrono>

class Timer {
public:
    Timer():
        start_(std::chrono::high_resolution_clock::now()) {
    }

    double seconds(bool reset = false) {
        auto now = std::chrono::high_resolution_clock::now();
        double result = std::chrono::duration_cast<std::chrono::milliseconds>(now - start_).count() / 1000.0;
        if (reset)
            start_ = now;
        return result;
    }

    static long SecondsSinceEpoch() {
        return std::chrono::system_clock::now().time_since_epoch() / std::chrono::milliseconds(1000);
    }

    static std::string HMS(double ms, bool displayFraction = true) {
        long seconds = (unsigned)ms;
        unsigned days = seconds / (3600 * 24);
        seconds = seconds % (3600 * 24);
        unsigned hours = seconds / 3600;
        seconds = seconds % 3600;
        unsigned minutes = seconds / 60;
        seconds = seconds % 60;
        std::stringstream s;
        s << days << ":" << std::setfill('0')
            << std::setw(2) << hours << ":"
            << std::setw(2) << minutes << ":"
            << std::setw(2) << seconds;
        if (displayFraction) {
            s << "." << std::setw(3) << (unsigned)((ms - seconds) * 1000);
        }
        return s.str();
    }

private:
    std::chrono::high_resolution_clock::time_point start_;
};

class ProgressReporter {
public:
    typedef std::function<void(ProgressReporter & p, std::ostream & s)> Feeder;

    static void Start(Feeder feeder, unsigned interval_ms = 1000) {
        std::thread t([feeder, interval_ms]() {
            ProgressReporter p(feeder);
            while (not p.allDone) {
                p.refresh();
                std::this_thread::sleep_for(std::chrono::milliseconds(interval_ms));
            }

        });
        t.detach();
    }

    long total = -1;
    long done = 0;
    long errors = 0;
    bool allDone = false;

private:

    static unsigned CountLines(std::string const & where) {
        unsigned result = 1;
        for (char c : where)
            if (c == '\n')
                ++result;
        return result;
    }

    ProgressReporter(Feeder feeder) :
        feeder_(feeder),
        start_(std::chrono::high_resolution_clock::now()) {
    }

    void refresh() {
        std::stringstream ss;
        feeder_(*this, ss);
        auto now = std::chrono::high_resolution_clock::now();
        double seconds = std::chrono::duration_cast<std::chrono::milliseconds>(now - start_).count() / 1000.0;
        std::cout << std::setw(16) << std::left << Timer::HMS(seconds, false);
        std::cout << "done " << std::setw(16) << done;

        if (errors != -1)
            std::cout << "errors " << std::setw(16) << errors;

        std::cout << std::endl;
        unsigned lines = 1;
        std::string extra = ss.str();
        if (not extra.empty()) {
            std::cout << extra << std::endl;
            lines += CountLines(extra);
        }
        std::cout << "\033[" << lines << "A";
    }

    Feeder feeder_;
    std::chrono::high_resolution_clock::time_point start_;
};

