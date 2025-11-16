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

enum class RejectionPolicy
{
    Abort,     // 抛出异常
    Discard,   // 丢弃新任务
    CallerRuns // 在提交任务的那个线程上直接执行该任务
};

class ThreadPool
{
public:
    // ================ 公共 API =====================

    ThreadPool();
    ~ThreadPool();

    void setMode(PoolMode mode);
    void setPolicy(RejectionPolicy policy);
    void setTaskQueMaxThreshHold(int threshhold);
    void setThreadSizeThreshHold(int threshhold);
    void start(int initThreadSize = std::thread::hardware_concurrency());
    void shutdown();

    int getCurrentThreadCount()const;
    int getIdleThreadCount()const;
    int getActiveThreadCount()const;
    size_t getTaskQueueSize();

    template <typename Func, typename... Args>
    auto submitTask(Func &&func, Args &&...args) -> std::future<decltype(func(args...))>
    {
        return submitTaskWithPriority(0, std::forward<Func>(func), std::forward<Args>(args)...);
    }

    template <typename Func, typename... Args>
    auto submitTaskWithPriority(int priority, Func &&func, Args &&...args) -> std::future<decltype(func(args...))>
    {
        using RType = decltype(func(args...));

        if (!isPoolRunning_)
        {
            throw std::runtime_error("ThreadPool is shutting down, no new tasks accepted.");
        }

        // auto task = std::make_shared<std::packaged_task<RType()>>(std::bind(std::forward<Func>(func), std::forward<Args>(args)...));
        auto bound_func = std::bind(std::forward<Func>(func), std::forward<Args>(args)...);
        auto packaged_task = std::packaged_task<RType()>(std::move(bound_func));
        std::future<RType> result = packaged_task.get_future();
        auto task_ptr = std::make_unique<ConcreteTask<RType>>(std::move(packaged_task));

        Thread *newThreadPtr = nullptr;
        std::unique_lock<std::mutex> lock(taskQueMtx_);

        if (!notFull.wait_for(lock, std::chrono::seconds(1),
                              [&]() -> bool
                              { return taskQue_.size() < (size_t)taskQueMaxThreshHold_; }))
        {
            switch (rejectionPolicy_)
            {
            case RejectionPolicy::Abort:
                std::cerr << "submit task timeout" << std::endl;
                throw std::runtime_error("Task queue is full...");

            case RejectionPolicy::Discard:
                std::cerr << "Task discarded" << std::endl;
                return result; 

            case RejectionPolicy::CallerRuns:
                std::cerr << "Task queue full, running in caller thread" << std::endl;
                lock.unlock();

                task_ptr->execute();
                return result;
            }
        }

        // 添加带权重的任务
        // auto taskFunc = std::make_shared<std::function<void()>>([task]() { (*task)(); });
        // taskQue_.emplace(taskFunc, priority);
        taskQue_.emplace(std::move(task_ptr), priority);
        notEmpty.notify_one();

        if (poolMode_ == PoolMode::MODE_CACHED &&
            taskQue_.size() > (size_t)idleThreadSize_ &&
            curThreadSize_ < threadSizeThreshHold_)
        {
            auto ptr = std::make_unique<Thread>(
                std::bind(&ThreadPool::threadFunc, this, std::placeholders::_1));
            int threadId = ptr->getId();
            newThreadPtr = ptr.get();
            threads_.emplace(threadId, std::move(ptr));
            curThreadSize_++;
        }

        lock.unlock();

        if (newThreadPtr != nullptr)
        {
            newThreadPtr->start();
        }

        return result;
    }

    ThreadPool(const ThreadPool &) = delete;
    ThreadPool &operator=(const ThreadPool &) = delete;

private:
    // ===============================================

    // --- Thread 类 ---
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
        int threadId_; // 自定义线程id以便回收
    };

    // --- ITask 接口 ---
    struct ITask
    {
        virtual ~ITask() = default;
        virtual void execute() = 0;
    };

    // --- ConcreteTask 实现 ---
    template <typename R>
    class ConcreteTask : public ITask
    {
    public:
        std::packaged_task<R()> task_;
        ConcreteTask(std::packaged_task<R()> &&task) : task_(std::move(task)) {}
        void execute() override
        {
            task_();
        }
    };

    // --- myTask 包装器  ---
    class myTask
    {
    public:
        int weight_; // 任务权重,权重越大优先级越高
        std::unique_ptr<ITask> task;

        myTask() = default;
        ~myTask() = default;

        myTask(std::unique_ptr<ITask> t, int w = 0)
            : weight_(w), task(std::move(t)) {}

        myTask(myTask &&other) noexcept
            : weight_(other.weight_), task(std::move(other.task)) {}

        myTask &operator=(myTask &&other) noexcept
        {
            weight_ = other.weight_;
            task = std::move(other.task);
            return *this;
        }
        bool operator<(const myTask &a) const
        {
            return weight_ < a.weight_; // 权重越大优先级越高
        }
        myTask(const myTask &) = delete;
        myTask &operator=(const myTask &) = delete;
    };

private:
    // ============= ThreadPool 成员 =================

    // 线程函数
    void threadFunc(int threadid);
    // 检查线程池运行状态
    bool checkRunningState() const;

    std::unordered_map<int, std::unique_ptr<Thread>> threads_;

    int initThreadSize_;
    int threadSizeThreshHold_;       // 线程数量上限
    std::atomic_int curThreadSize_;  // 当前线程数量
    std::atomic_int idleThreadSize_; // 空闲线程数量

    std::priority_queue<myTask> taskQue_;
    int taskQueMaxThreshHold_; // 任务数量上限

    std::mutex taskQueMtx_;
    std::condition_variable notFull;
    std::condition_variable notEmpty;
    std::condition_variable exitCond_;

    PoolMode poolMode_;
    RejectionPolicy rejectionPolicy_ = RejectionPolicy::Abort;

    std::atomic_bool isPoolRunning_;
};

#endif // THREADPOOL_H