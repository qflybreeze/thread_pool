#ifndef THREADPOOL_H
#define THREADPOOL_H

#include <vector>
#include <queue>
#include <memory>
#include <atomic>
#include <mutex>
#include <condition_variable>
#include <functional>
#include <unordered_map>
#include <thread>
#include <future>
#include <iostream>

enum class PoolMode
{
    MODE_FIXED, // 线程数固定
    MODE_CACHED // 线程数动态增长
};

class Thread
{
public:
    using ThreadFunc = std::function<void(int)>;
    Thread(ThreadFunc func);
    ~Thread() = default;
    void start();

    int getId() const;

private:
    ThreadFunc func_;
    static std::atomic_int generateId_;
    int threadId_; // 保存线程id以便回收
};

class ThreadPool
{
public:
    ThreadPool();
    ~ThreadPool();

    void setMode(PoolMode mode);

    void setTaskQueMaxThreshHold(int threshhold);

    void setThreadSizeThreshHold(int threshhold);

    template <typename Func, typename... Args>
    auto submitTask(Func &&func, Args &&...args) -> std::future<decltype(func(args...))>
    {
        using RType = decltype(func(args...));
        auto task = std::make_shared<std::packaged_task<RType()>>(
            std::bind(std::forward<Func>(func), std::forward<Args>(args)...));
        std::future<RType> result = task->get_future();

        Thread *newThreadPtr = nullptr;
        // 获取锁
        std::unique_lock<std::mutex> lock(taskQueMtx_);
        // 等待任务队列有空余
        if (!notFull.wait_for(lock, std::chrono::seconds(1),
                              [&]() -> bool
                              { return taskQue_.size() < (size_t)taskQueMaxThreshHold_; }))
        {
            std::cerr << "submit task timeout" << std::endl;
            if constexpr (std::is_void_v<RType>)
            {
                auto time_out_task = std::make_shared<std::packaged_task<void()>>([]() {});
                (*time_out_task)();
                return time_out_task->get_future();
            }
            else
            {
                auto time_out_task = std::make_shared<std::packaged_task<RType()>>(
                    []() -> RType
                    { return RType(); });
                (*time_out_task)();
                return time_out_task->get_future();
            }
        }
        // 添加任务
        taskQue_.emplace([task]()
                         { (*task)(); });
        // 通知线程处理任务
        notEmpty.notify_one();

        // cache模式
        if (poolMode_ == PoolMode::MODE_CACHED && taskQue_.size() > (size_t)idleThreadSize_ && curThreadSize_ < threadSizeThreshHold_)
        {
            // 创建新线程
            auto ptr = std::make_unique<Thread>(std::bind(&ThreadPool::threadFunc, this, std::placeholders::_1));
            int threadId = ptr->getId();
            newThreadPtr = ptr.get();
            threads_.emplace(threadId, std::move(ptr));

            curThreadSize_++;
        }

        lock.unlock(); // 先释放锁,在锁外启动新线程

        if (newThreadPtr != nullptr)
        {
            newThreadPtr->start();
        }

        return result;
    }

    void start(int initThreadSize = std::thread::hardware_concurrency());

    ThreadPool(const ThreadPool &) = delete;
    ThreadPool &operator=(const ThreadPool &) = delete;

private:
    // 线程函数
    void threadFunc(int threadid);
    // 检查线程池运行状态
    bool checkRunningState() const;

private:
    // std::vector<std::unique_ptr<Thread>> threads_;
    std::unordered_map<int, std::unique_ptr<Thread>> threads_;

    int initThreadSize_;
    int threadSizeThreshHold_;       // 线程数量上限
    std::atomic_int curThreadSize_;  // 当前线程数量
    std::atomic_int idleThreadSize_; // 空闲线程数量

    using Task = std::function<void()>;
    std::queue<Task> taskQue_;
    int taskQueMaxThreshHold_; // 任务数量上限

    std::mutex taskQueMtx_;
    std::condition_variable notFull;
    std::condition_variable notEmpty;
    std::condition_variable exitCond_;

    PoolMode poolMode_;

    std::atomic_bool isPoolRunning_;
};

#endif // THREADPOOL_H