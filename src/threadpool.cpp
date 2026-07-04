#include "threadpool.hpp"

ThreadPool::ThreadPool(size_t num_threads) : active_workers(0), stop(false) {
    for(size_t i = 0; i < num_threads; i++) {
        workers.emplace_back(&ThreadPool::worker, this);
    }
}

ThreadPool::~ThreadPool() {
    {
        lock_guard<mutex> lock(queue_mutex);
        stop = true;
    }
    cv.notify_all();

    for(auto &t: workers) {
        t.join();
    }
}

void ThreadPool::wait() {
    unique_lock<mutex> lock(queue_mutex);

    finished_cv.wait(lock, [this] {
        return jobs.empty() && active_workers == 0;
    });
}

void ThreadPool::enqueue(function<void()> job) {
    {
        lock_guard<mutex> lock(queue_mutex);
        jobs.push(move(job));
    }
    cv.notify_one();
}


void ThreadPool::worker() {
    while(true) {
        function<void()> job;
        {
            unique_lock<mutex> lock(queue_mutex);
            cv.wait(lock, [this] {
                return !jobs.empty() || stop;
            });

            if(stop && jobs.empty()) return;
    
            job = move(jobs.front()); 
            ++active_workers;
            jobs.pop();
        }

        job();
        --active_workers;
        
        lock_guard<mutex> lock(queue_mutex);
        if(jobs.empty() && active_workers == 0) {
            finished_cv.notify_all();
        }
    }
}