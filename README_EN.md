# C++ Priority Thread Pool

English | [简体中文](README.md)

A feature-rich, high-performance C++ thread pool implementation based on the C++17 standard, using `std::future`, `std::packaged_task`, and `std::priority_queue` to provide flexible task scheduling and asynchronous result retrieval.

See [test.cpp](https://github.com/qflybreeze/thread_pool/blob/New-architecture/test.cpp) for usage examples.

## 🌟 Core Features

* **Two Working Modes**:
    * `MODE_FIXED` (default): Fixed number of threads.
    * `MODE_CACHED`: Dynamic thread count that automatically creates new threads based on workload (with limits) and recycles idle threads after a timeout.
* **Priority Tasks**:
    * Support submitting tasks with priorities using `submitTaskWithPriority`.
    * Internally uses a priority queue (`std::priority_queue`) to ensure high-priority tasks are always executed first.
* **Asynchronous Results**:
    * Returns task execution results using `std::future`.
    * Supports any callable object (function pointers, lambdas, `std::function`) with any number of arguments.
* **Flexible Rejection Policies**:
    * When the task queue is full, provides three rejection policies (`RejectionPolicy`):
        * `Abort` (default): Throws a `std::runtime_error` exception.
        * `Discard`: Silently discards the newly submitted task.
        * `CallerRuns`: Executes the task directly on the caller's thread.
* **Graceful Shutdown**:
    * The `shutdown()` method waits for all queued tasks to complete before smoothly stopping all worker threads.
* **Runtime Monitoring**:
    * Provides APIs to get the current total thread count, idle thread count, active thread count, and number of tasks in the queue.

## 📚 API Overview

### 1. ThreadPool Class

#### Main Methods

* `ThreadPool()`: Constructor.
* `start(int initThreadSize)`: Starts the thread pool. `initThreadSize` is the initial number of threads.
* `shutdown()`: Gracefully shuts down the thread pool. The destructor also calls it automatically.
* `submitTask(Func&& func, Args&&... args)`: Submits a task with default priority (0).
* `submitTaskWithPriority(int priority, Func&& func, Args&&... args)`: Submits a task with a priority. Higher `priority` means higher precedence.

#### Configuration Methods (must be called before start())

* `setMode(PoolMode mode)`: Sets the thread pool mode (`MODE_FIXED` or `MODE_CACHED`).
* `setPolicy(RejectionPolicy policy)`: Sets the task rejection policy.
* `setTaskQueMaxThreshHold(int threshhold)`: Sets the maximum capacity of the task queue.
* `setThreadSizeThreshHold(int threshhold)`: Sets the maximum number of threads in `MODE_CACHED` mode.

#### Monitoring Methods

* `getCurrentThreadCount() const`: Gets the current total number of threads.
* `getIdleThreadCount() const`: Gets the current number of idle threads.
* `getActiveThreadCount() const`: Gets the current number of active (executing tasks) threads.
* `getTaskQueueSize()`: Gets the number of pending tasks in the queue.

### 2. Enums

#### PoolMode

* `MODE_FIXED`
* `MODE_CACHED`

#### RejectionPolicy

* `Abort`
* `Discard`
* `CallerRuns`

## 🔧 Thread Pool Modes

### MODE_FIXED

* Thread count remains constant after initialization
* Suitable for predictable, stable workloads
* Lower overhead, easier to manage

### MODE_CACHED

* Dynamically adjusts thread count based on workload
* Creates new threads when tasks exceed idle threads
* Automatically recycles threads idle for more than 60 seconds
* Maximum thread count limited by `threadSizeThreshHold_`
* Suitable for fluctuating workloads

## 🚫 Rejection Policies

When the task queue is full, the thread pool applies the configured rejection policy:

### Abort (Default)

```cpp
pool.setPolicy(RejectionPolicy::Abort);
```

Throws a `std::runtime_error` exception, requiring the caller to handle it.

### Discard

```cpp
pool.setPolicy(RejectionPolicy::Discard);
```

Silently discards the task without notification.

### CallerRuns

```cpp
pool.setPolicy(RejectionPolicy::CallerRuns);
```

Executes the task synchronously on the caller's thread, providing natural backpressure.

## 📊 Monitoring and Statistics

```cpp
// Get current thread count
int totalThreads = pool.getCurrentThreadCount();

// Get idle thread count
int idleThreads = pool.getIdleThreadCount();

// Get active thread count
int activeThreads = pool.getActiveThreadCount();

// Get pending task count
size_t pendingTasks = pool.getTaskQueueSize();
```

## ⚙️ Implementation Details

* **Task Encapsulation**: Uses `std::packaged_task` to wrap tasks, supporting return values and exception handling
* **Priority Queue**: Tasks are stored in `std::priority_queue`, sorted by priority
* **Thread Safety**: All shared state protected by `std::mutex` and `std::condition_variable`
* **Dynamic Scaling**: In `MODE_CACHED`, threads are created on-demand and recycled when idle
* **Graceful Shutdown**: `shutdown()` ensures all queued tasks complete before thread termination

## 📝 Notes

* Configuration methods (`setMode`, `setPolicy`, etc.) must be called **before** `start()`
* The destructor automatically calls `shutdown()` for safe resource cleanup
* Task priorities are integers; higher values indicate higher priority
* Tasks with the same priority are executed in FIFO order
* In `MODE_CACHED`, threads idle for more than 60 seconds are automatically recycled (only when thread count exceeds initial count)

## 🛠️ Compilation and Dependencies

* **C++17** or higher compiler (e.g., `g++` or `clang++`)
* **pthread** (required on Linux/macOS)

### Compilation Example

Assuming you have a `main.cpp` that uses this thread pool:

```bash
g++ -std=c++17 main.cpp threadpool.cpp -o my_app -lpthread
```

On Windows with MinGW/MSYS2:

```bash
g++ -std=c++17 main.cpp threadpool.cpp -o my_app.exe
```

## 🤝 Contributing

Issues and pull requests are welcome!
