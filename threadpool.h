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

class ITask
{
public:
    virtual ~ITask() = default;
    virtual void execute() = 0;
};

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

class myTask
{
public:
    int weight_; // 任务权重,权重越大优先级越高
    std::unique_ptr<ITask> task;
    myTask() = default;
    myTask(std::unique_ptr<ITask> t, int w = 0);
    myTask(myTask &&other) noexcept;
    ~myTask() = default;
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

class ThreadPool
{
public:
    ThreadPool();
    ~ThreadPool();

    void setMode(PoolMode mode);

    void setTaskQueMaxThreshHold(int threshhold);

    void setThreadSizeThreshHold(int threshhold);

    template <typename Func, typename... Args>
    auto submitTask(Func &&func, Args &&...args) -> std::future<decltype(func(args...))>;

    template <typename Func, typename... Args>
    auto submitTaskWithPriority(int priority, Func &&func, Args &&...args) -> std::future<decltype(func(args...))>;

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

    // using Task = std::function<void()>;
    std::priority_queue<myTask> taskQue_;
    int taskQueMaxThreshHold_; // 任务数量上限

    std::mutex taskQueMtx_;
    std::condition_variable notFull;
    std::condition_variable notEmpty;
    std::condition_variable exitCond_;

    PoolMode poolMode_;

    std::atomic_bool isPoolRunning_;
};

template <typename Func, typename... Args>
auto ThreadPool::submitTask(Func &&func, Args &&...args) -> std::future<decltype(func(args...))>
{
    return submitTaskWithPriority(0, std::forward<Func>(func), std::forward<Args>(args)...);
}

template <typename Func, typename... Args>
auto ThreadPool::submitTaskWithPriority(int priority, Func &&func, Args &&...args) -> std::future<decltype(func(args...))>
{
    using RType = decltype(func(args...));
    // auto task = std::make_shared<std::packaged_task<RType()>>(std::bind(std::forward<Func>(func), std::forward<Args>(args)...));
    auto bound_func = std::bind(std::forward<Func>(func), std::forward<Args>(args)...);
    auto packaged_task = std::packaged_task<RType()>(std::move(bound_func));

    auto task_ptr = std::make_unique<ConcreteTask<RType>>(std::move(packaged_task));

    std::future<RType> result = task->get_future();

    Thread *newThreadPtr = nullptr;
    std::unique_lock<std::mutex> lock(taskQueMtx_);

    if (!notFull.wait_for(lock, std::chrono::seconds(1),
                          [&]() -> bool
                          { return taskQue_.size() < (size_t)taskQueMaxThreshHold_; }))
    {
        std::cerr << "submit task timeout" << std::endl;
        throw std::runtime_error("Task queue is full, submit task timeout after 1 second");
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
#endif // THREADPOOL_H