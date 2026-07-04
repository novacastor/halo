#pragma once
#include <thread>
#include <mutex>
#include <condition_variable>
#include <queue>
#include <vector>
#include <functional>
#include <atomic>

using namespace std;

class ThreadPool {
public:
    ThreadPool(size_t num_threads);
    ~ThreadPool();
    void enqueue(function<void()> job);
    void wait();

private:
    mutex queue_mutex;

    condition_variable cv;
    condition_variable finished_cv;

    vector<thread> workers;
    queue<function<void()>> jobs;
    
    atomic<int> active_workers;
    bool stop;
    void worker();
};