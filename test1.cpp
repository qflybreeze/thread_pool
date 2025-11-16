#include "threadpool.h"
#include <iostream>

int sum(int a, int b)
{
    return a + b;
}

int main()
{
    ThreadPool pool;
    pool.start(4);

    // 提交不同优先级的任务
    pool.submitTaskWithPriority(10, []()
                                { std::cout << "High priority task (10)" << std::endl; });

    pool.submitTaskWithPriority(1, []()
                                { std::cout << "Low priority task (1)" << std::endl; });

    pool.submitTaskWithPriority(5, []()
                                { std::cout << "Medium priority task (5)" << std::endl; });

    std::this_thread::sleep_for(std::chrono::seconds(1));

    return 0;
}