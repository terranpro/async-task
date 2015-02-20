#ifndef AS_TASK_CONTEXT_HPP
#define AS_TASK_CONTEXT_HPP

#include "TaskFunction.hpp"

namespace as {

class TaskContext
{
public:
	virtual ~TaskContext() {}

	virtual void Invoke() = 0;
	virtual void Yield() = 0;
};

class TaskContextBase
	: public TaskContext
{
protected:
	std::unique_ptr<TaskFunctionBase> taskfunc;

	TaskContextBase(std::unique_ptr<TaskFunctionBase> func)
		: taskfunc(std::move(func))
	{}

public:
	bool IsFinished() const
	{
		return taskfunc->IsFinished();
	}
};

} // namespace as

#endif // AS_TASK_CONTEXT_HPP
