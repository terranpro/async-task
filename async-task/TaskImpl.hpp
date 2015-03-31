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

#include "TaskControlBlock.hpp"

#include <memory>
#include <functional>

namespace as {

class TaskImplBase
{
public:
	virtual ~TaskImplBase() {}

	virtual void Invoke() = 0;
	virtual void Yield() = 0;
	virtual void Cancel() = 0;
	virtual bool IsFinished() const = 0;
};

template<class Ret>
class TaskImpl
	: public TaskImplBase
{
protected:
	std::shared_ptr< TaskControlBlock<Ret> > ctrl;

public:
	TaskImpl() = default;

	template<class Func, class... Args>
	TaskImpl( Func&& func, Args&&... args )
		: ctrl( std::make_shared< TaskControlBlock<Ret> >(
			        std::bind(std::forward<Func>(func), std::forward<Args>(args)... ) ) )
	{}

	virtual ~TaskImpl()
	{}

	virtual void Invoke()
	{
		ctrl->Run();
	}

	virtual void Yield()
	{}

	virtual void Cancel()
	{
		ctrl->Cancel();
	}

	virtual bool IsFinished() const
	{
		return ctrl && ctrl->IsFinished();
	}

	std::shared_ptr< TaskControlBlock<Ret> > GetControlBlock() const
	{
		return ctrl;
	}
};

template< template<class> class TaskType, class Func, class... Args>
std::shared_ptr< TaskType<decltype( std::declval<Func>()( std::declval<Args>()... ) )> >
make_task(Func&& func, Args&&... args)
{
	typedef decltype( std::declval<Func>()( std::declval<Args>()... ) )
		result_type;

	return std::make_shared< TaskType<result_type> >(
		std::forward<Func>(func),
		std::forward<Args>(args)... );
}

} // namespace as

#endif // AS_TASK_IMPL_HPP
