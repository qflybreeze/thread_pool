#include "threadpool.h"
#include <functional>
#include <thread>
#include <iostream>

const int TASK_MAX_THRESHHOLD = INT32_MAX;
const int THREAD_MAX_THRESHHOLD = 1024;
const int THREAD_MAX_IDLE_TIME = 60; // 单位：秒

ThreadPool::ThreadPool()
    : initThreadSize_(0),
      idleThreadSize_(0),
      curThreadSize_(0),
      taskQueMaxThreshHold_(TASK_MAX_THRESHHOLD),
      threadSizeThreshHold_(THREAD_MAX_THRESHHOLD),
      poolMode_(PoolMode::MODE_FIXED),
      isPoolRunning_(false) {}

ThreadPool::~ThreadPool()
{
    {
        std::unique_lock<std::mutex> lock(taskQueMtx_);
        isPoolRunning_ = false;
        notEmpty.notify_all();
    }
    std::unique_lock<std::mutex> lock(taskQueMtx_);
    exitCond_.wait(lock, [&]() -> bool
                   { return threads_.size() == 0; });
}

void ThreadPool::setMode(PoolMode mode)
{
    if (checkRunningState())
    {
        return;
    }
    poolMode_ = mode;
}

void ThreadPool::setTaskQueMaxThreshHold(int threshhold)
{
    if (checkRunningState())
    {
        return;
    }
    taskQueMaxThreshHold_ = threshhold;
}

void ThreadPool::setThreadSizeThreshHold(int threshhold)
{
    if (checkRunningState())
    {
        return;
    }
    if (poolMode_ == PoolMode::MODE_CACHED)
    {
        threadSizeThreshHold_ = threshhold;
    }
}

void ThreadPool::start(int initThreadSize)
{
    // 设置线程池运行状态
    isPoolRunning_ = true;
    // 记录初始线程个数
    initThreadSize_ = initThreadSize;
    curThreadSize_ = initThreadSize;

    // 创建线程对象
    for (int i = 0; i < initThreadSize_; i++)
    {
        auto ptr = std::make_unique<Thread>(std::bind(&ThreadPool::threadFunc, this, std::placeholders::_1));
        int threadId = ptr->getId();
        threads_.emplace(threadId, std::move(ptr));
    }
    // 启动线程
    for (auto &pair : threads_)
    {
        pair.second->start();
    }
}

void ThreadPool::threadFunc(int threadid)
{
    auto lastTime = std::chrono::high_resolution_clock::now();

    while (true)
    {
        Task task;
        {
            // 获取锁
            std::unique_lock<std::mutex> lock(taskQueMtx_);

            idleThreadSize_++;

            // 等待任务或停止信号
            while (taskQue_.size() == 0)
            {
                // 检查是否应该停止
                if (!isPoolRunning_)
                {
                    idleThreadSize_--;
                    threads_.erase(threadid);
                    curThreadSize_--;
                    std::cout << "threadid:" << std::this_thread::get_id() << " exit (pool stopped)" << std::endl;
                    exitCond_.notify_all();
                    return;
                }

                if (poolMode_ == PoolMode::MODE_CACHED)
                {
                    // cached模式下，空闲线程等待时间超过指定时间则结束该线程
                    if (std::cv_status::timeout ==
                        notEmpty.wait_for(lock, std::chrono::seconds(1)))
                    {
                        auto now = std::chrono::high_resolution_clock::now();
                        auto dur = std::chrono::duration_cast<std::chrono::seconds>(now - lastTime);
                        if (dur.count() >= THREAD_MAX_IDLE_TIME && curThreadSize_ > initThreadSize_)
                        {
                            // 回收线程
                            threads_.erase(threadid);
                            curThreadSize_--;
                            idleThreadSize_--;
                            std::cout << "threadid:" << std::this_thread::get_id() << " exit" << std::endl;
                            exitCond_.notify_all();
                            return;
                        }
                    }
                }
                else
                {
                    // 等待任务队列非空
                    notEmpty.wait(lock);
                }
            }

            idleThreadSize_--;
            // 获取任务
            task = std::move(taskQue_.front());
            taskQue_.pop();

            // 通知其他线程还有任务
            if (taskQue_.size() > 0)
            {
                notEmpty.notify_one();
            }

            // 通知生产者任务队列有空余
            notFull.notify_one();
        } // 释放锁

        // 执行任务
        if (task)
        {
            task();
        }
        lastTime = std::chrono::high_resolution_clock::now();
    }
}

bool ThreadPool::checkRunningState() const
{
    return isPoolRunning_;
}

////////Thread
std::atomic_int Thread::generateId_ = 0;

Thread::Thread(ThreadFunc func) : func_(func), threadId_(generateId_.fetch_add(1)) {}

void Thread::start()
{
    std::thread t(func_, threadId_);
    t.detach();
}

int Thread::getId() const
{
    return threadId_;
}
