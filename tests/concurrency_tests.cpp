#include "indexing/threadpool.hpp"
#include "indexing/workQueue.hpp"

#include <gtest/gtest.h>

#include <atomic>

TEST(WorkQueue, PushPopAndTryPopPreserveFifoOrder) {
    WorkQueue<int> queue;
    queue.push(10);
    queue.push(20);

    EXPECT_EQ(queue.pop(), 10);
    int next = 0;
    ASSERT_TRUE(queue.try_pop(next));
    EXPECT_EQ(next, 20);
    EXPECT_FALSE(queue.try_pop(next));
}

TEST(ThreadPool, WaitReturnsAfterAllQueuedJobsFinish) {
    ThreadPool pool(3);
    std::atomic<int> completed{0};

    for (int i = 0; i < 100; ++i) {
        pool.enqueue([&completed] { ++completed; });
    }
    pool.wait();

    EXPECT_EQ(completed.load(), 100);
}
