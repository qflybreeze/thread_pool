# C++ Priority Thread Pool

[English](README_EN.md) | 简体中文

这是一个功能丰富的、高性能的 C++ 线程池实现。它基于 C++17 标准，使用 `std::future`、`std::packaged_task` 和 `std::priority_queue` 来提供灵活的任务调度和异步结果检索。

使用示例可见 [test.cpp](https://github.com/qflybreeze/thread_pool/blob/New-architecture/test.cpp)。

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

## 📚 API 概览

### 1. ThreadPool 类

#### 主要方法

* `ThreadPool()`: 构造函数。
* `start(int initThreadSize)`: 启动线程池。`initThreadSize` 是初始线程数。
* `shutdown()`: 优雅地关闭线程池。析构函数也会自动调用它。
* `submitTask(Func&& func, Args&&... args)`: 提交一个默认优先级 (0) 的任务。
* `submitTaskWithPriority(int priority, Func&& func, Args&&... args)`: 提交一个带优先级的任务。`priority` 越大，优先级越高。

#### 配置方法 (必须在 start() 之前调用)

* `setMode(PoolMode mode)`: 设置线程池模式 (`MODE_FIXED` 或 `MODE_CACHED`)。
* `setPolicy(RejectionPolicy policy)`: 设置任务拒绝策略。
* `setTaskQueMaxThreshHold(int threshhold)`: 设置任务队列的最大容量。
* `setThreadSizeThreshHold(int threshhold)`: 设置 `MODE_CACHED` 模式下的最大线程数。

#### 监控方法

* `getCurrentThreadCount() const`: 获取当前线程总数。
* `getIdleThreadCount() const`: 获取当前空闲线程数。
* `getActiveThreadCount() const`: 获取当前活动（正在执行任务）的线程数。
* `getTaskQueueSize()`: 获取任务队列中待处理的任务数。

### 2. 枚举

#### PoolMode

* `MODE_FIXED`
* `MODE_CACHED`

#### RejectionPolicy

* `Abort`
* `Discard`
* `CallerRuns`

## 🔧 线程池模式

### MODE_FIXED

* 初始化后线程数量保持不变
* 适用于可预测的稳定工作负载
* 开销较低，更易于管理

### MODE_CACHED

* 根据工作负载动态调整线程数量
* 当任务数超过空闲线程数时创建新线程
* 自动回收空闲超过 60 秒的线程
* 最大线程数受 `threadSizeThreshHold_` 限制
* 适用于波动性工作负载

## 🚫 拒绝策略

当任务队列已满时，线程池会应用配置的拒绝策略：

### Abort（默认）

```cpp
pool.setPolicy(RejectionPolicy::Abort);
```

抛出 `std::runtime_error` 异常，需要调用者处理。

### Discard

```cpp
pool.setPolicy(RejectionPolicy::Discard);
```

静默丢弃任务，不进行任何通知。

### CallerRuns

```cpp
pool.setPolicy(RejectionPolicy::CallerRuns);
```

在调用者线程上同步执行任务，提供自然的反压机制。

## 📊 监控与统计

```cpp
// 获取当前线程数
int totalThreads = pool.getCurrentThreadCount();

// 获取空闲线程数
int idleThreads = pool.getIdleThreadCount();

// 获取活动线程数
int activeThreads = pool.getActiveThreadCount();

// 获取待处理任务数
size_t pendingTasks = pool.getTaskQueueSize();
```

## ⚙️ 实现细节

* **任务封装**: 使用 `std::packaged_task` 包装任务，支持返回值和异常处理
* **优先队列**: 任务存储在 `std::priority_queue` 中，按优先级排序
* **线程安全**: 所有共享状态由 `std::mutex` 和 `std::condition_variable` 保护
* **动态扩展**: 在 `MODE_CACHED` 模式下，按需创建线程并在空闲时回收
* **优雅停机**: `shutdown()` 确保所有已排队的任务完成后才终止线程

## 📝 注意事项

* 配置方法（`setMode`、`setPolicy` 等）必须在 `start()` **之前**调用
* 析构函数会自动调用 `shutdown()` 以安全清理资源
* 任务优先级为整数；数值越大，优先级越高
* 相同优先级的任务按 FIFO 顺序执行
* 在 `MODE_CACHED` 模式下，空闲超过 60 秒的线程会自动回收（仅当线程数超过初始数量时）

## 🛠️ 编译与依赖

* **C++17** 或更高版本的编译器 (例如 `g++` 或 `clang++`)
* **pthread** (在 Linux/macOS 上需要)

### 编译示例

假设您有一个 `main.cpp` 来使用此线程池：

```bash
g++ -std=c++17 main.cpp threadpool.cpp -o my_app -lpthread
```

在 Windows 上使用 MinGW/MSYS2：

```bash
g++ -std=c++17 main.cpp threadpool.cpp -o my_app.exe
```

## 🤝 贡献

欢迎提交 Issues 和 Pull Requests！