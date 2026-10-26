#ifndef THREADPOOL_H
#define THREADPOOL_H

#include <vector>
#include <thread>
#include <queue>
#include <condition_variable>
#include <mutex>
#include <functional>
#include <string>
#include <atomic>
#include <optional>
#include <chrono>
#include <cstddef>

class threadPool
{
private:
    std::vector<std::thread> workers;   //工作线程容量
    std::queue<std::function<void()>> tasks;    //任务队列容量
    std::mutex task_mutex;              //保护任务队列的互斥锁
    std::condition_variable task_cv;    //线程同步的条件变量
    const size_t maxQueueSize;          //任务队列最大容量
    std::atomic<bool> stop{ false };    //停止标志（原子操作）
    std::atomic<int> pendingTasks{ 0 }; //待处理任务数

    void startThreadPool(size_t numThreads);

public:
    // 构造函数增加最大队列容量参数（默认1024）
    explicit threadPool(size_t threadPoolSize, size_t maxQueueSize = 1024);
    ~threadPool();

    // 添加任务（带超时机制）
    bool addTask(std::function<void()> task, std::chrono::seconds timeout = std::chrono::seconds(10));

    // 阻塞等待所有工作线程退出（配合进程优雅关闭：在 Server/DB 仍存活时调用，避免析构竞态）
    void join();

    // 停止接收新任务并回收所有工作线程：置停止标志 -> 唤醒全部阻塞线程 -> 等待其退出。
    // 注意：不能只调用 join()——空闲工作线程阻塞在条件变量上，若 stop 未置位则永远
    // 不会被唤醒，join() 将永久死锁（进程无法自行退出，只能被 SIGKILL）。
    void shutdown();

    // 查询状态
    size_t getPendingTasks() const { return static_cast<size_t>(pendingTasks); }
    bool isFull() const { return static_cast<size_t>(pendingTasks) >= maxQueueSize; }
};
#endif