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
#include "TaskFunction.hpp"
#include "TaskStatus.hpp"
#include "Channel.hpp"

#ifdef AS_USE_COROUTINE_TASKS
#include "CoroutineTaskContext.hpp"
#endif // AS_USE_COROUTINE_TASKS

namespace as {

template<class Ret>
struct TaskResultControlBlockBase
{
	typedef Ret& result_type;
	typedef Ret& reference_type;

	std::unique_ptr<Ret> result;

	template<class Func, class Promise>
	void RunHelper( Func& task_func, Promise& promise )
	{
		result.reset( new Ret{ task_func() } );
		promise.set_value( *result );
	}

	bool IsSet() const
	{
		return result != nullptr;
	}
};

template<>
struct TaskResultControlBlockBase<void>
{
	typedef void result_type;
	typedef void reference_type;

	std::atomic<bool> is_set;

	TaskResultControlBlockBase()
		: is_set(false)
	{}

	template<class Func, class Promise>
	void RunHelper( Func& task_func, Promise& promise )
	{
		task_func();

		promise.set_value();
		is_set = true;
	}

	bool IsSet() const
	{
		return is_set;
	}
};

template<class T>
struct TaskResultControlBlock
	: public TaskResultControlBlockBase<T>
{
	typedef TaskResultControlBlockBase<T> base_type;

	typedef typename base_type::result_type result_type;
	typedef typename base_type::reference_type reference_type;

	std::promise<reference_type> result_promise;
	std::shared_future<reference_type> result_future;

	TaskResultControlBlock()
		: result_promise()
		, result_future( result_promise.get_future() )
	{}

	template<class Func>
	void Run(Func& task_func)
	{
		if ( IsFinished() )
			return;

		base_type::RunHelper( task_func, result_promise );
	}

	void Cancel()
	{
		decltype(result_future) invalidate;
		result_future = std::move(invalidate);
	}

	bool Valid() const
	{
		return result_future.valid();
	}

	bool IsFinished() const
	{
		return Valid() == false || base_type::IsSet();
	}

	result_type Get()
	{
		return result_future.get();
	}

	void Wait()
	{
		result_future.wait();
	}

	template<class Rep, class Period>
	WaitStatus WaitFor( std::chrono::duration<Rep,Period> const& dur ) const
	{
		return static_cast<WaitStatus>( static_cast<int>( result_future.wait_for( dur ) ) );
	}
};

template<class T>
struct TaskResultControlBlock< TaskFuncResult<T> >
{
	typedef typename Channel<T>::result_type result_type;

	Channel<T> channel;

	TaskResultControlBlock() = default;

	template<class Func>
	void Run(Func& task_func)
	{
		if ( !channel.IsOpen() )
			return;

		TaskFuncResult<T> fr = task_func();

		if ( fr.status == TaskStatus::Finished ||
		     fr.status == TaskStatus::Continuing ) {

			channel.Put( std::move(fr) );

			if ( fr.status == TaskStatus::Finished )
				channel.Close();

		} else if ( fr.status == TaskStatus::Canceled ) {

			channel.Cancel();
		}
	}

	void Cancel()
	{
		channel.Cancel();
	}

	bool Valid() const
	{
		return channel.IsOpen();
	}

	bool IsFinished() const
	{
		return !channel.IsOpen();
	}

	result_type
	Get()
	{
		return channel.Get();
	}

	void Wait()
	{
		channel.Wait();
	}

	template<class Rep, class Period>
	WaitStatus WaitFor( std::chrono::duration<Rep,Period> const& dur ) const
	{
		return channel.WaitFor( dur );
	}
};

template<class T>
class TaskResult
{
	std::shared_ptr< TaskResultControlBlock<T> > ctrl;

	template<class U> friend struct TaskFunction;

private:
	TaskResult( std::shared_ptr<TaskResultControlBlock<T> > ctrl)
		: ctrl(ctrl)
	{}

public:
	TaskResult() = default;

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

	TaskResult<Ret> GetResult()
	{
		return { ctrl };
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
