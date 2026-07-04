#include "concurrentQueue.hpp"

void WorkQueue::push(IndexJob job) {
    {
        lock_guard<mutex> lock(mtx);
        jobs.push(move(job));
    }
    cv.notify_one();
}

IndexJob WorkQueue::pop() {
    unique_lock<mutex> lock(mtx);
    cv.wait(lock, [this] {
        return !jobs.empty();
    });

    IndexJob job = move(jobs.front());
    jobs.pop();
    return job;
}
bool WorkQueue::try_pop(IndexJob& job) {
    lock_guard<mutex> lock(mtx);
    if (jobs.empty()) return false;
    
    job = move(jobs.front());
    jobs.pop();
    return true;
}