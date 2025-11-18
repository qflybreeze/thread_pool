# C++ Priority Thread Pool

这是一个功能丰富的、高性能的 C++ 线程池实现。它基于 C++17 标准，使用 `std::future`、`std::packaged_task` 和 `std::priority_queue` 来提供灵活的任务调度和异步结果检索。

## 🌟 核心特性

* **两种工作模式**:
    * `MODE_FIXED` (默认): 线程数量固定。
    * `MODE_CACHED`: 线程数量动态增长，可根据任务量自动创建新线程（有上限），并在线程空闲过久后自动回收。
* **优先级任务**:
    * 支持使用 `submitTaskWithPriority` 提交带优先级的任务。
    * 线程池内部使用优先队列 (`std::priority_queue`)，确保高优先级的任务总是被优先执行。
* **异步结果**:
    * 使用 `std::future` 返回任务的执行结果。
    * 支持任意可调用对象（函数指针、lambda、`std::function`）和任意数量的参数。
* **灵活的拒绝策略**:
    * 当任务队列已满时，提供三种拒绝策略 (`RejectionPolicy`)：
        * `Abort` (默认): 抛出 `std::runtime_error` 异常。
        * `Discard`: 静默丢弃新提交的任务。
        * `CallerRuns`: 在提交任务的调用者线程上直接执行该任务。
* **优雅停机**:
    * `shutdown()` 方法会等待所有已在队列中的任务执行完毕，然后平稳地停止所有工作线程。
* **运行时监控**:
    * 提供 API 获取当前线程总数、空闲线程数、活动线程数以及任务队列中的任务数量。

## 🛠️ 编译与依赖

* **C++17** 或更高版本的编译器 (例如 `g++` 或 `clang++`)
* **pthread** (在 Linux/macOS 上需要)

### 编译示例

假设您有一个 `main.cpp` 来使用此线程池：

```bash
g++ -std=c++17 main.cpp threadpool.cpp -o my_app -lpthread