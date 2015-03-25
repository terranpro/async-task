//
//  TaskFunction.hpp - Task function abstraction
//
//  Copyright (c) 2015 Brian Fransioli
//
//  Distributed under the Boost Software License, Version 1.0.
//  See accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt
//

#ifndef AS_TASK_FUNCTION_HPP
#define AS_TASK_FUNCTION_HPP

#include "TaskControlBlock.hpp"

#include <memory>

namespace as {

struct TaskFunctionBase
{
	virtual ~TaskFunctionBase() {}

	virtual void Run() = 0;
	virtual void Cancel() = 0;
	virtual bool IsFinished() const = 0;
};

template<class Ret>
struct TaskFunction
	: public TaskFunctionBase
{
	std::shared_ptr< TaskResultControlBlock<Ret> > ctrl;
	std::function<Ret()> task_func;

	template<class Func, class... Args>
	TaskFunction( Func&& func, Args&&... args )
		: ctrl( CreateControlBlock() )
		, task_func( std::bind(std::forward<Func>(func), std::forward<Args>(args)... ) )
	{}

	void Run()
	{
		ctrl->Run( task_func );
	}

	void Cancel()
	{
		ctrl->Cancel();
	}

	bool IsFinished() const
	{
		return ctrl && ctrl->IsFinished();
	}

	std::shared_ptr< TaskResultControlBlock<Ret> >
	GetControlBlock()
	{
		return ctrl;
	}

private:
	std::shared_ptr< TaskResultControlBlock<Ret> > CreateControlBlock() const
	{
		return std::make_shared< TaskResultControlBlock<Ret> >();
	}
};

template<class Func, class... Args>
std::unique_ptr< TaskFunction<decltype( std::declval<Func>()( std::declval<Args>()... ) )> >
make_task_function(Func&& func, Args&&... args)
{
	typedef decltype( std::declval<Func>()( std::declval<Args>()... ) )
		result_type;

	std::unique_ptr<TaskFunction<result_type> > taskfunc{ new TaskFunction< result_type >(
			std::forward<Func>(func),
			std::forward<Args>(args)... ) };

	return taskfunc;
}

} // namespace as

#endif // AS_TASK_FUNCTION_HPP
