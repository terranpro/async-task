//
//  Task.hpp - Asynchronous Tasks and TaskResult<>'s
//
//  Copyright (c) 2015 Brian Fransioli
//
//  Distributed under the Boost Software License, Version 1.0.
//  See accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt
//

#ifndef AS_TASK_HPP
#define AS_TASK_HPP

#include <memory>
#include <future>
#include <vector>
#include <functional>

// TODO: enable when platform boost supports context
//#undef AS_USE_COROUTINE_TASKS

#include "TaskContext.hpp"
#include "TaskControlBlock.hpp"
#include "TaskFunction.hpp"
#include "TaskStatus.hpp"
#include "Channel.hpp"

#ifdef AS_USE_COROUTINE_TASKS
#include "CoroutineTaskContext.hpp"
#endif // AS_USE_COROUTINE_TASKS

namespace as {

template<class T>
class TaskResult
{
	std::shared_ptr< TaskResultControlBlock<T> > ctrl;

public:
	TaskResult() = default;

	TaskResult( std::shared_ptr<TaskResultControlBlock<T> > ctrl)
		: ctrl(ctrl)
	{}

	typename TaskResultControlBlock<T>::result_type
	Get()
	{
		return ctrl->Get();
	}

	bool Valid() const
	{
		return ctrl && ctrl->Valid();
	}

	void Wait() const
	{
		return ctrl->Wait();
	}

	template<class Rep, class Period>
	WaitStatus WaitFor( std::chrono::duration<Rep,Period> const& dur ) const
	{
		return ctrl->WaitFor( dur );
	}
};

class Task
{
private:
	std::shared_ptr<TaskContext> context;

public:
	struct GenericTag {};
	struct CoroutineTag {};

public:
	Task() = default;

	// Auto deduced Generic
	template<class Func, class... Args,
	         typename = typename std::enable_if<
		         !std::is_base_of< typename std::remove_reference<Func>::type, Task >::value &&
		         !std::is_same< typename std::remove_reference<Func>::type,
		                        std::shared_ptr<TaskFunctionBase>
		                         >::value
	                                           >::type >

	Task(Func&& func, Args&&... args)
		: context{ std::make_shared<TaskContext>(
			         make_task_function(std::forward<Func>(func), std::forward<Args>(args)...) ) }
	{}

	template<class TFunc,
	         typename = typename std::enable_if<
		         std::is_base_of< TaskFunctionBase,
		                          typename std::remove_reference<TFunc>::type
		                        >::value
	                                           >::type
	        >
	Task(std::unique_ptr<TFunc> tf)
		: context( std::make_shared<TaskContext>(std::move(tf)) )
	{}

	// Explicitly Generic
	template<class Func, class... Args>
	Task(GenericTag, Func&& func, Args&&... args)
		: context{ std::make_shared<TaskContext>(
			make_task_function(std::forward<Func>(func), std::forward<Args>(args)...) ) }
	{}

	template<class TFunc>
	Task(GenericTag, std::unique_ptr<TFunc> tf)
		: context( std::make_shared<TaskContext>( std::move(tf) ) )
	{}

#ifdef AS_USE_COROUTINE_TASKS
	// Explicity Coroutine
	template<class Func, class... Args>
	Task(CoroutineTag, Func&& func, Args&&... args)
		: context( std::make_shared<CoroutineTaskContext>(
			           make_task_function(std::forward<Func>(func), std::forward<Args>(args)...) ) )
	{}


	template<class TFunc,
	         typename = typename std::enable_if<
		         std::is_base_of< TaskFunctionBase,
		                          typename std::remove_reference<TFunc>::type
		                        >::value
	                                           >::type
	        >
	Task(CoroutineTag, std::unique_ptr<TFunc> tf)
		: context( std::make_shared<CoroutineTaskContext>( std::move(tf) ) )
	{}
#endif // AS_USE_COROUTINE_TASKS

	~Task() = default;

	Task(Task&&) = default;
	Task(Task const&) = default;

	Task& operator=(Task&&) = default;
	Task& operator=(Task const&) = default;

	void Invoke()
	{
		context->Invoke();
	}

	void Yield()
	{
		context->Yield();
	}

	bool IsFinished() const
	{
		return context->IsFinished();
	}

	void Cancel()
	{
		context->Cancel();
	}
};

} // namespace as

#endif // AS_TASK_HPP
