//
//  TaskContext.hpp - Task Context abstraction
//
//  Copyright (c) 2015 Brian Fransioli
//
//  Distributed under the Boost Software License, Version 1.0.
//  See accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt
//

#ifndef AS_TASK_CONTEXT_HPP
#define AS_TASK_CONTEXT_HPP

#include "TaskFunction.hpp"

namespace as {

class TaskContextBase
{
public:
	virtual ~TaskContextBase() {}

	virtual void Invoke() = 0;
	virtual void Yield() = 0;
	virtual void Cancel() = 0;
};

class TaskContext
	: public TaskContextBase
{
protected:
	std::unique_ptr<TaskFunctionBase> taskfunc;

public:
	TaskContext() = default;

	TaskContext(std::unique_ptr<TaskFunctionBase> func)
		: taskfunc(std::move(func))
	{}

	virtual ~TaskContext()
	{}

	bool IsFinished() const
	{
		return taskfunc->IsFinished();
	}

	virtual void Invoke()
	{
		taskfunc->Run();
	}

	virtual void Yield()
	{}

	virtual void Cancel()
	{
		taskfunc->Cancel();
	}
};

} // namespace as

#endif // AS_TASK_CONTEXT_HPP
