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

#include <boost/pool/pool_alloc.hpp>

namespace as {

class TaskImpl
{
public:
	virtual ~TaskImpl() {}

	virtual TaskStatus Invoke() = 0;
	virtual void Yield() = 0;
	virtual void Cancel() = 0;
	//virtual bool IsFinished() const = 0;
};

// template<class Invoker, class Result>

template< class Handler >
class TaskImplBase
	: public TaskImpl
{
protected:
	Handler handler;

public:
	TaskImplBase() = default;

	template<class Func, class... Args,
	         class =
	         typename std::enable_if< !std::is_same<TaskImplBase,
	                                                typename std::decay<Func>::type
	                                               >::value
	                                >::type
	        >
	TaskImplBase( Func&& func, Args&&... args )
		: handler( std::bind( std::forward<Func>(func),
		                      std::forward<Args>(args)... ) )
	{}

	TaskImplBase(TaskImplBase&&) = default;

	virtual ~TaskImplBase()
	{}

	virtual TaskStatus Invoke()
	{
		return handler();
	}

	virtual void Yield()
	{}

	virtual void Cancel()
	{
	}

};

struct PostInvoker
	: public TaskImpl
{
	std::unique_ptr<callable> func;
	//callable_ptr< boost::pool_allocator<callable> > func;

	// template<class Func, class... Args>
	// PostInvoker(Func&& func, Args&&... args)
	template<class Func>
	PostInvoker(Func&& func)
		: func( make_callable( std::forward<Func>(func) ) )
	{}

	virtual TaskStatus Invoke()
	{
		(*func)();
		return TaskStatus::Finished;
	}

	virtual void Yield()
	{}
	virtual void Cancel()
	{}
};

} // namespace as

#endif // AS_TASK_IMPL_HPP
