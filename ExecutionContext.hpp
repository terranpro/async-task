#ifndef AS_EXECUTION_CONTEXT_HPP
#define AS_EXECUTION_CONTEXT_HPP

#include "Task.hpp"

#include <functional>
#include <memory>
#include <chrono>

namespace as {

class ExecutionContext
{
public:
	virtual ~ExecutionContext() {}

	virtual void AddTask(Task task) = 0;
	virtual void AddTimedTask(Task task, std::chrono::milliseconds time_ms) = 0;

	virtual void Iteration() = 0;
	virtual bool IsCurrent() = 0;
};

} // namespace as

#endif // AS_EXECUTION_CONTEXT_HPP
