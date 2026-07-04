#pragma once
#include "tokenizer.hpp"
#include <queue>
#include <mutex>
#include <condition_variable>

using namespace std;

struct IndexJob {
    string path;
    vector<Engine::TokenMatch> tokens;
    bool is_poison_pill = false;
    long long mtime;
};

class WorkQueue {
public:
    void push(IndexJob job);
    IndexJob pop();
    bool try_pop(IndexJob& job);

private:
    queue<IndexJob> jobs;
    mutex mtx;
    condition_variable cv;
};