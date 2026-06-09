#ifndef THREADPOOL_H
#define THREADPOOL_H
#include "MCCS.h"

class ThreadPool {

public:

    using task = std::function<void()>;

    ThreadPool(int num_threads) : stop(false) {
        for (int i = 0; i < num_threads; ++i) {
            workers.emplace_back(&ThreadPool::worker, this);
        }
    }

    ~ThreadPool() {
        {
            std::unique_lock<std::mutex> lock(queue_mutex);
            stop = true;
        }
        cv.notify_all();
        for (std::thread &worker : workers) {
            if(worker.joinable()) {
                worker.join();
            }
        }
    }

    void post(task t) {
        {
            std::unique_lock<std::mutex> lock(queue_mutex);
            tasks.push(std::move(t));
        }
        cv.notify_one();
    }

private:
    
    std::mutex queue_mutex;
    std::condition_variable cv;
    
    std::queue<task> tasks;
    std::vector<std::thread> workers;
    std::atomic<bool> stop;

    void worker() {
        while (true) {
            task cur;
            {
                std::unique_lock<std::mutex> lock(queue_mutex);
                cv.wait(lock, [this] { return !tasks.empty() || stop;});
                if (stop && tasks.empty()) {
                    return;
                }
                cur = std::move(tasks.front());
                tasks.pop();
            }
            cur();
        }
    }
};


#endif
