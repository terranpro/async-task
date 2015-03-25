//
//  TaskImpl.hpp - Basic Task Context/Implementation
//
//  Copyright (c) 2015 Brian Fransioli
//
//  Distributed under the Boost Software License, Version 1.0.
//  See accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt
//

#ifndef AS_TASK_IMPL_HPP
#define AS_TASK_IMPL_HPP

#include "TaskFunction.hpp"

namespace as {

class TaskImplBase
{
public:
	virtual ~TaskImplBase() {}

	virtual void Invoke() = 0;
	virtual void Yield() = 0;
	virtual void Cancel() = 0;
};

class TaskImpl
	: public TaskImplBase
{
protected:
	std::unique_ptr<TaskFunctionBase> taskfunc;

public:
	TaskImpl() = default;

	TaskImpl(std::unique_ptr<TaskFunctionBase> func)
		: taskfunc(std::move(func))
	{}

	virtual ~TaskImpl()
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

#endif // AS_TASK_IMPL_HPP
