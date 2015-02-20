#ifndef AS_EXECUTOR_HPP
#define AS_EXECUTOR_HPP

#include "Task.hpp"

#include <functional>
#include <memory>
#include <chrono>

namespace as {

class Executor
{
public:
	virtual ~Executor() {}

	virtual void Schedule(Task task) = 0;
	virtual void ScheduleAfter(Task task, std::chrono::milliseconds time_ms) = 0;

	virtual void Iteration() = 0;
	virtual bool IsCurrent() = 0;

	// TODO: can this be done...
	// static Executor& GetCurrent()
	// {

	// }
};

} // namespace as

#endif // AS_EXECUTOR_HPP
