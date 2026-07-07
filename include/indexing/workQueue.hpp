#pragma once
#include <queue>
#include <mutex>
#include <condition_variable>

template<typename T>
class WorkQueue {
public:
    void push(T job) {
        std::lock_guard<std::mutex> lock(mtx);
        jobs.emplace(std::move(job));
        cv.notify_one();
    }

    T pop() {
        std::unique_lock<std::mutex> lock(mtx);
        cv.wait(lock, [this] {
            return !jobs.empty();
        });
        T job = std::move(jobs.front()); 
        jobs.pop(); 
        return job;
    }

    bool try_pop(T& job) {
        std::lock_guard<std::mutex> lock(mtx);
        if (jobs.empty()) return false;
        
        job = std::move(jobs.front());
        jobs.pop();
        return true;
    }

private:
    std::queue<T> jobs;
    std::mutex mtx;
    std::condition_variable cv;
};