#pragma once
#include <thread>
#include <mutex>
#include <condition_variable>
#include <queue>
#include <atomic>
#include <cmath>

#include "utils.h"



int threadId();

void setThreadId(int value);

void Write(std::string what);

void Error(std::string what);

void Log(std::string what);


template<typename CRTP, typename TASK>
class Worker {
public:

    static size_t BlockingTaskQueueSize;


    /** Schedules given task to be processed by the worker.
     */
    static void Schedule(TASK const & task, bool blockIfFull = true) {
        std::unique_lock<std::mutex> g(m_);
        if (blockIfFull == true) {
            while (tasks_.size() >= BlockingTaskQueueSize) {
                // block unitil there is room in the queue
                cvFull_.wait(g);
            }
        }
        tasks_.push(task);
        cvNotEmpty_.notify_one();
    }

    /** Creates numThreads threads, each of which will run a worker's instance.

      When spawned, all threads immediately block, even if there are new tasks in the queue and wait blocked until the run() method is called.
     */
    static void Spawn(unsigned numThreads) {
        if (running_)
            throw std::runtime_error("Unable to Spawn threads, already running");
        numThreads_ = numThreads;
        threads_.resize(numThreads);
        for (unsigned i = 0; i < numThreads; ++i) {
            std::thread t([i] () {
                // set thread id
                setThreadId(i);
                // create the worker
                CRTP worker;
                threads_[i] = &worker;
                // start the worker
                worker.start();
                Log("Done.");
                // wake up the blocked Stop() if last thread finishes
                if (--numThreads_ == 0)
                    cvStatus_.notify_all();
                threads_[i] = nullptr;
            });
            t.detach();
        }
    }

    /** Runs the spawned threads.
     */
    static void Run() {
        if (running_ == true)
            return;
        // flip to running
        running_ = true;
        start_ = std::chrono::high_resolution_clock::now();
        // notify all threads
        cvStatus_.notify_all();
    }

    /** Stops all threads, i.e. raises a stop flag and waits for all threads to stop, at which point it returns to caller.
     */
    static void Stop() {
        std::unique_lock<std::mutex> g(m_); // first get the mutex
        // set the stop flag
        running_ = false;
        // notify all threads in case they are waiting on the empty queue
        cvNotEmpty_.notify_all();
        // wait until the last thread finishes
        while (numThreads_ > 0)
            cvStatus_.wait(g);
        totalTime_ = TimeSinceStart();
    }

    /** Waits for all tasks to become blocked on an empty queue. Then stops them and returns when done.
     */
    static void Wait() {
        std::unique_lock<std::mutex> g(m_); // get the mutex
        while (not tasks_.empty() or runningThreads_ > 0) {
            // wait for the status update, i.e. when a thread waits for
            cvStatus_.wait(g);
        }
        // the queue is empty, all tasks are waiting, initiate stop
        running_ = false;
        // wakeup all threads so that they can exit
        cvNotEmpty_.notify_all();
        // wait for the threads to exit
        while (numThreads_ > 0)
            cvStatus_.wait(g);
        totalTime_ = TimeSinceStart();
    }

    /** Returns true when all threads have finished their execution. 
     */
    static bool AllDone() {
        return numThreads_ == 0;
    }

    static unsigned long CompletedTasks() {
        return completedTasks_;
    }

    static unsigned long ErrorTasks() {
        return errorTasks_;
    }

    static double TotalTime() {
        if (not std::isnan(totalTime_))
            return totalTime_;
        return TimeSinceStart();
    }


protected:
    static std::vector<CRTP *> threads_;

private:

    class WorkerTerminatedException {
    };

    static double TimeSinceStart() {
        auto now = std::chrono::high_resolution_clock::now();
        return std::chrono::duration_cast<std::chrono::milliseconds>(now - start_).count() / 1000.0;
    }

    /** This method must be overriden in children to process the given task.
     */
    virtual void run(TASK & task) = 0;


    TASK getNextTask() {
        std::unique_lock<std::mutex> g(m_);
        // if the job queue is empty, wait
        while (tasks_.empty()) {
            --runningThreads_;
            // notify status change because the thread is going to sleep
            cvStatus_.notify_one();
            cvNotEmpty_.wait(g);
            // when we wake up, check that we are still good to go
            if (not running_)
                throw WorkerTerminatedException();
            if (not tasks_.empty()) {
                // increase number of running threads and get the job
                ++runningThreads_;
                break;
            }
        }
        TASK task = tasks_.front();
        // if there is a room for new task, and there was none before, notify producers
        if (tasks_.size() == BlockingTaskQueueSize)
            cvFull_.notify_one();
        tasks_.pop();
        return task;
    }

    void start() {
        Log("Started.");
        // initially, wait for the running_ flag to be set to true
        {
            std::unique_lock<std::mutex> g(m_);
            while (not running_) {
                cvStatus_.wait(g);
            }
            // increase number of running threads
            ++runningThreads_;
        }
        Log("Running.");
        // while we are in running state, get task to process and run on it.
        while (running_ == true) {
            try {
                TASK task = getNextTask();
                try {
                    // we got the task, run it
                    run(task);
                    // this cannot throw so is safe
                    ++completedTasks_;
                } catch (std::exception const & e) {
                    Error(STR(e.what() << " while executing task " << task));
                    ++completedTasks_;
                    ++errorTasks_;
                }
            } catch (WorkerTerminatedException) {
                return;
            }
        }
    }

    // CV's to signal

    /** CV variable that is used to signal any event change, i.e. full queue, etc.
     */
    static std::condition_variable cvNotEmpty_;


    static std::condition_variable cvFull_;

    static std::condition_variable cvStatus_;

    /** Queue access mutex.
     */
    static std::mutex m_;

    /** Queue of available tasks.
     */
    static std::queue<TASK> tasks_;

    /** Number of existing threads.

      Initialized to numThreads by the Spawn() method, decreased by threads as they are stopped.
     */
    static unsigned numThreads_;

    /** Number of active threads (i.e. threads that are not blocking on empty task queue.
     */
    static unsigned runningThreads_;

    /** If true, the threads should be running. If false, they should stop if running, or wait for run if they has not started yet.
     */
    static bool running_;

    /** Number of tasks completed (including error tasks)
     */
    static std::atomic<uint64_t> completedTasks_;

    /** Number of tasks which have failed.
     */
    static std::atomic<uint64_t> errorTasks_;

    /** Start of the execution.
    */
    static std::chrono::high_resolution_clock::time_point start_;

    /** Duration of the task.
    */
    static double totalTime_;
};



template<typename CRTP, typename TASK>
std::vector<CRTP *> Worker<CRTP, TASK>::threads_;

template<typename CRTP, typename TASK>
size_t Worker<CRTP, TASK>::BlockingTaskQueueSize = 1000;

template<typename CRTP, typename TASK>
std::condition_variable Worker<CRTP, TASK>::cvNotEmpty_;

template<typename CRTP, typename TASK>
std::condition_variable Worker<CRTP, TASK>::cvFull_;

template<typename CRTP, typename TASK>
std::condition_variable Worker<CRTP, TASK>::cvStatus_;

template<typename CRTP, typename TASK>
std::mutex Worker<CRTP, TASK>::m_;

template<typename CRTP, typename TASK>
std::queue<TASK> Worker<CRTP, TASK>::tasks_;

template<typename CRTP, typename TASK>
unsigned Worker<CRTP, TASK>::numThreads_ = 0;

template<typename CRTP, typename TASK>
unsigned Worker<CRTP, TASK>::runningThreads_ = 0;

template<typename CRTP, typename TASK>
bool Worker<CRTP, TASK>::running_ = false;

template<typename CRTP, typename TASK>
std::atomic<uint64_t> Worker<CRTP, TASK>::completedTasks_(0);

template<typename CRTP, typename TASK>
std::atomic<uint64_t> Worker<CRTP, TASK>::errorTasks_(0);

template<typename CRTP, typename TASK>
std::chrono::high_resolution_clock::time_point Worker<CRTP, TASK>::start_;

template<typename CRTP, typename TASK>
double Worker<CRTP, TASK>::totalTime_(NAN);






