#include"threadpool.h"
#include <functional>

//const int DEFAULT_WEIGHT = 0;

myTask::myTask(std::unique_ptr<ITask> t, int w=0)
    : weight_(w), task(std::move(t)) {}

myTask::myTask(myTask&& other) noexcept
    : weight_(other.weight_), task(std::move(other.task)) {}