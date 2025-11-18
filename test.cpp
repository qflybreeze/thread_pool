#include "threadpool.h"
#include <iostream>
#include <chrono>
#include <stdexcept>
#include <string>

using namespace std::chrono_literals;

// 辅助函数：打印当前线程ID和任务名
void log_task(const std::string& task_name) {
    std::cout << "  [Task: " << task_name << "] executed by thread: " 
              << std::this_thread::get_id() << std::endl;
}

// 辅助函数：打印线程池状态
void print_pool_status(ThreadPool& pool, const std::string& title) {
    std::cout << "\n--- " << title << " Status ---\n"
              << "  Total Threads: " << pool.getCurrentThreadCount() << "\n"
              << "  Idle Threads:  " << pool.getIdleThreadCount() << "\n"
              << "  Active Threads: " << pool.getActiveThreadCount() << "\n"
              << "  Task Queue Size: " << pool.getTaskQueueSize() << "\n"
              << "-------------------------\n" << std::endl;
}

int main()
{
    std::cout << "Main thread ID: " << std::this_thread::get_id() << std::endl;

    // ==========================================================
    // 测试 1: FIXED 模式 和 任务优先级
    // ==========================================================
    std::cout << "\n=========== TEST 1: FIXED Mode & Priority ===========\n";
    {
        ThreadPool pool;
        pool.setMode(PoolMode::MODE_FIXED);
        pool.start(2); // 启动2个线程

        print_pool_status(pool, "Test 1 Start");

        // 提交4个低优先级任务 (权重 1)
        for (int i = 0; i < 4; ++i) {
            pool.submitTaskWithPriority(1, [i]() {
                std::this_thread::sleep_for(100ms);
                log_task("LOW Priority " + std::to_string(i));
            });
        }

        // 提交2个高优先级任务 (权重 10)
        auto f1 = pool.submitTaskWithPriority(10, []() {
            log_task("HIGH Priority A");
            return 100;
        });
        auto f2 = pool.submitTaskWithPriority(10, []() {
            log_task("HIGH Priority B");
            return 200;
        });

        // 期望：HIGH A 和 HIGH B 会先于所有 LOW 任务执行
        std::cout << "Waiting for HIGH priority results..." << std::endl;
        int total = f1.get() + f2.get();
        std::cout << "HIGH priority tasks returned sum: " << total << std::endl;
        
        // 析构函数将自动调用 shutdown() 并等待所有任务完成
    }
    std::cout << "Test 1 Pool destroyed.\n";


    // ==========================================================
    // 测试 2: 拒绝策略 (Abort, Discard, CallerRuns)
    // ==========================================================
    std::cout << "\n=========== TEST 2: Rejection Policies ===========\n";
    {
        ThreadPool pool_reject;
        pool_reject.setMode(PoolMode::MODE_FIXED);
        pool_reject.setTaskQueMaxThreshHold(1); // 队列容量 = 1
        pool_reject.start(1); // 工作线程 = 1

        // 1. 提交一个“阻塞”任务，占满唯一的线程
        auto blocker = pool_reject.submitTask([] {
            log_task("Blocker Task (runs for 2s)");
            std::this_thread::sleep_for(2s);
        });
        
        std::this_thread::sleep_for(50ms); // 确保 "Blocker" 任务已开始执行
        print_pool_status(pool_reject, "Blocker running");

        // 2. 提交一个任务，占满唯一的队列
        pool_reject.submitTask([] { log_task("Queued Task"); });
        print_pool_status(pool_reject, "Queue is full");
        
        // 此时：1个线程在忙，1个任务在排队。线程池已满。

        // 3. 测试 RejectionPolicy::Abort (默认)
        std::cout << "\nTesting ABORT Policy (default)..." << std::endl;
        try {
            // 此任务将等待1秒，然后超时并抛出异常
            pool_reject.submitTask([] { log_task("ABORT (Should not run)"); });
        } catch (const std::runtime_error& e) {
            std::cout << "  SUCCESS: Caught expected exception: " << e.what() << std::endl;
        }

        // 4. 测试 RejectionPolicy::Discard
        std::cout << "\nTesting DISCARD Policy..." << std::endl;
        pool_reject.setPolicy(RejectionPolicy::Discard);
        try {
            // 此任务将等待1秒，然后超时并被丢弃 (不抛异常)
            pool_reject.submitTask([] { log_task("DISCARD (Should not run)"); });
            std::cout << "  SUCCESS: Task submitted and discarded (no exception)." << std::endl;
        } catch (...) {
            std::cout << "  FAILURE: Discard policy threw an exception!" << std::endl;
        }

        // 5. 测试 RejectionPolicy::CallerRuns
        std::cout << "\nTesting CALLERRUNS Policy..." << std::endl;
        pool_reject.setPolicy(RejectionPolicy::CallerRuns);
        try {
            // 此任务将等待1秒，然后超时并在 *main* 线程上执行
            auto f_caller = pool_reject.submitTask([]() {
                log_task("CALLERRUNS Task");
                return "Executed by Main Thread!";
            });
            // .get() 会立即返回，因为任务在上面一行同步执行了
            std::cout << "  SUCCESS: CallerRuns task returned: " << f_caller.get() << std::endl;
        } catch (...) {
            std::cout << "  FAILURE: CallerRuns policy threw an exception!" << std::endl;
        }

        blocker.get(); // 等待阻塞任务完成
        pool_reject.shutdown(); // 显式关闭
    }
    std::cout << "Test 2 Pool destroyed.\n";

    
    // ==========================================================
    // 测试 3: CACHED 模式 (线程动态增长)
    // ==========================================================
    std::cout << "\n=========== TEST 3: CACHED Mode ===========\n";
    {
        ThreadPool pool_cached;
        pool_cached.setMode(PoolMode::MODE_CACHED);
        pool_cached.setThreadSizeThreshHold(10); // 最多10个线程
        pool_cached.start(2); // 初始 2 个线程

        print_pool_status(pool_cached, "Cached Pool Start");
        
        std::vector<std::future<void>> futures;
        // 提交10个任务，迫使线程池创建新线程
        for (int i = 0; i < 10; ++i) {
            futures.push_back(pool_cached.submitTask([i] {
                log_task("Cached Task " + std::to_string(i));
                std::this_thread::sleep_for(500ms);
            }));
        }

        std::this_thread::sleep_for(100ms); // 等待线程创建
        print_pool_status(pool_cached, "Cached Pool Peak Load");
        std::cout << "  (Expected thread count > 2, up to 10)" << std::endl;

        // 等待所有任务完成
        for(auto& f : futures) {
            f.get();
        }
        std::cout << "All cached tasks finished." << std::endl;
        print_pool_status(pool_cached, "Cached Pool Idle");
        std::cout << "  (Idle threads will be reaped after 60s...)\n";
    }
    std::cout << "Test 3 Pool destroyed.\n";

    std::cout << "\n=========== ALL TESTS PASSED ===========\n";
    return 0;
}