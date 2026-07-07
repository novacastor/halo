#pragma once
#include <thread>
#include <mutex>
#include <condition_variable>
#include <queue>
#include <vector>
#include <functional>
#include <atomic>

class ThreadPool {
public:
    ThreadPool(size_t num_threads) : active_workers(0), stop(false) {
        for(size_t i = 0; i < num_threads; i++) {
            workers.emplace_back(&ThreadPool::worker, this);
        }
    }
    ~ThreadPool() {
        {
            std::lock_guard<std::mutex> lock(queue_mutex);
            stop = true;
        }
        cv.notify_all();
    
        for(auto &t: workers) {
            t.join();
        }
    }
    void enqueue(std::function<void()> job) {
        {
            std::lock_guard<std::mutex> lock(queue_mutex);
            jobs.push(std::move(job));
        }
        cv.notify_one();
    }
    void wait() {
        std::unique_lock<std::mutex> lock(queue_mutex);
    
        finished_cv.wait(lock, [this] {
            return jobs.empty() && active_workers == 0;
        });
    }

private:
    std::mutex queue_mutex;

    std::condition_variable cv;
    std::condition_variable finished_cv;

    std::vector<std::thread> workers;
    std::queue<std::function<void()>> jobs;
    
    std::atomic<int> active_workers;
    bool stop;

    void worker() {
        while(true) {
            std::function<void()> job;
            {
                std::unique_lock<std::mutex> lock(queue_mutex);
                cv.wait(lock, [this] {
                    return !jobs.empty() || stop;
                });
    
                if(stop && jobs.empty()) return;
        
                job = std::move(jobs.front()); 
                ++active_workers;
                jobs.pop();
            }
    
            job();
            --active_workers;
            
            std::lock_guard<std::mutex> lock(queue_mutex);
            if(jobs.empty() && active_workers == 0) {
                finished_cv.notify_all();
            }
        }
    }
};





