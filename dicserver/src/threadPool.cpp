#include "threadPool.h"
#include <iostream>
#include <chrono>

using namespace std;

threadPool::threadPool(size_t threadPoolSize, size_t maxQueueSize)
    : maxQueueSize(maxQueueSize), stop(false)
{
    cout << "Initializing thread pool with " << threadPoolSize 
    << " threads, max queue size: " << maxQueueSize << endl;
    startThreadPool(threadPoolSize);
}

threadPool::~threadPool()
{
    {
        unique_lock<mutex> lock(task_mutex);
        stop = true;
        pendingTasks = 0;
    }
    task_cv.notify_all();

    for (auto& worker : workers) {
        if (worker.joinable()) {
            worker.join();
        }
    }
}
/*
1.首先检查线程池是否已停止，如果已停止则返回false
2.如果队列已满，等待一段时间，直到队列有空闲或者超时
3.如果等待期间线程池停止，则返回false
4.如果等待期间超时，也返回false
5.再次检查线程池是否已停止，如果已停止则返回false
6.最后将任务加入队列，增加待处理任务数量，并通知一个工作线程
*/
bool threadPool::addTask(function<void()> task, std::chrono::seconds timeout)
{
    {
        unique_lock<mutex> lock(task_mutex);
        if (stop) return false;

        // 队列满时等待
        if (tasks.size() >= maxQueueSize) {
            if (!task_cv.wait_for(lock, timeout, [this] {
                return tasks.size() < maxQueueSize || stop;
                })) {
                return false; // 超时或已停止
            }
        }

        if (stop) return false;

        tasks.push(std::move(task));
        pendingTasks++;
    }

    task_cv.notify_one();
    return true;
}
/*
1.创建指定数量的线程，每个线程都执行一个循环：等待条件变量，直到有任务或者线程池停止
2.如果线程池停止且任务队列为空，则线程退出
3.否则从任务队列取出任务，执行任务，并捕获异常
4.任务执行完成后，通知所有等待的生产者
*/
void threadPool::join()
{
    for (auto &worker : workers)
    {
        if (worker.joinable())
        {
            worker.join();
        }
    }
}

// 优雅关闭收尾：先置停止标志并唤醒全部阻塞中的工作线程，再等待它们退出。
// - 空闲工作线程：被 notify_all 唤醒后看到 stop 且队列为空，立即返回；
// - 正在执行任务的工作线程：把当前任务跑完（连接收发的超时即其上界）后回到循环，
//   此时 stop 已置位，同样立即返回。
// 因此整体等待有上界，不会无限卡住。
void threadPool::shutdown()
{
    {
        std::unique_lock<std::mutex> lock(task_mutex);
        stop = true;
        pendingTasks = 0;
    }
    task_cv.notify_all();
    join();
}

void threadPool::startThreadPool(size_t numThreads)
{
    workers.reserve(numThreads);
    for (size_t i = 0; i < numThreads; ++i) {
        workers.emplace_back([this] {
            while (true) {
                std::function<void()> task;
                {
                    unique_lock<mutex> lock(task_mutex);
                    task_cv.wait(lock, [this] {
                        return stop || !tasks.empty();
                        });

                    if (stop && tasks.empty()) {
                        return;
                    }

                    if (!tasks.empty()) {
                        task = std::move(tasks.front());
                        tasks.pop();
                        pendingTasks--;
                    }
                }

                if (task) {
                    try {
                        task();
                    }
                    catch (const std::exception& e) {
                        std::cerr << "Task failed with exception: " 
                        << e.what() << std::endl;
                    }
                    task_cv.notify_all(); // 通知等待的生产者
                }
            }
            });
    }
}