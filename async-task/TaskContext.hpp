//
//  bind.hpp - binds function objects to arguments
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
