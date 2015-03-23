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

#include <iostream>

// TODO: enable when platform boost supports context
//#undef AS_USE_COROUTINE_TASKS

#include "TaskContext.hpp"
#include "TaskFunction.hpp"

#ifdef AS_USE_COROUTINE_TASKS
#include "CoroutineTaskContext.hpp"
#endif // AS_USE_COROUTINE_TASKS

namespace as {

enum class TaskStatus {
	Finished,
	Repeat,
	Continuing,
	Canceled
};

template<class T>
struct TaskFuncResult;

template<>
struct TaskFuncResult<void>;

template<>
struct TaskFuncResult<void>
{
	TaskStatus status;

	TaskFuncResult()
		: status()
	{}

	TaskFuncResult(TaskStatus s)
		: status(s)
	{}

	template<class U>
	operator TaskFuncResult<U>()
	{
		return TaskFuncResult<U>(*this);
	}
};

template<class T>
struct TaskFuncResult
{
	TaskStatus status;
	std::unique_ptr<T> ret;

	TaskFuncResult()
		: status()
		, ret()
	{}

	TaskFuncResult(TaskStatus s)
		: status(s)
		, ret()
	{}

	TaskFuncResult(TaskStatus s, T val)
		: status(s)
		, ret( new T{ val } )
	{}

	explicit TaskFuncResult(T val)
		: status()
		, ret( new T{ val } )
	{}

	TaskFuncResult(TaskFuncResult<void> const& other)
		: status(other.status)
		, ret()
	{}
};

static const TaskFuncResult<void> repeat{ TaskStatus::Repeat };
static const TaskFuncResult<void> cancel{ TaskStatus::Canceled };

template<class T>
TaskFuncResult< typename std::remove_reference<T>::type >
finished(T&& res)
{
	return TaskFuncResult< typename std::remove_reference<T>::type >( TaskStatus::Finished, std::forward<T>(res) );
}

template<class T>
TaskFuncResult< typename std::remove_reference<T>::type >
continuing(T&& res)
{
	return TaskFuncResult< typename std::remove_reference<T>::type >( TaskStatus::Continuing, std::forward<T>(res) );
}

enum WaitStatus {
	Deferred = static_cast<int>( std::future_status::deferred ),
	Ready = static_cast<int>( std::future_status::ready ),
	Timeout = static_cast<int>( std::future_status::timeout )
};

template<class T>
struct TaskResultControlBlock
{
	typedef T& result_type;

	std::unique_ptr<T> result;
	std::promise<T&> result_promise;
	std::shared_future<T&> result_future;

	TaskResultControlBlock()
		: result()
		, result_promise()
		, result_future( result_promise.get_future() )
	{}

	template<class Func>
	void Run(Func& task_func)
	{
		if ( IsFinished() )
			return;

		result.reset( new T{ task_func() } );

		result_promise.set_value( *result );
	}

	void Cancel()
	{
		std::cout << "CANCELED!\n";

		decltype(result_future) invalidate;
		result_future = std::move(invalidate);
	}

	bool Valid() const
	{
		return result_future.valid();
	}

	bool IsFinished() const
	{
		return Valid() == false || result != nullptr;
	}

	result_type Get()
	{
		return result_future.get();
	}
};

template<>
struct TaskResultControlBlock<void>
{
	typedef void result_type;

	std::promise<void> result_promise;
	std::shared_future<void> result_future;

	TaskResultControlBlock()
		: result_promise()
		, result_future( result_promise.get_future() )
	{}

	void Run(std::function<void()>& task_func)
	{
		if ( IsFinished() )
			return;

		task_func();

		result_promise.set_value();
	}

	void Cancel()
	{
		std::cout << "CANCELED!\n";
		decltype(result_future) invalidate;
		result_future = std::move(invalidate);
	}

	bool Valid() const
	{
		return result_future.valid();
	}

	bool IsFinished() const
	{
		if ( !Valid() )
			return true;

		return ( result_future.wait_for( std::chrono::nanoseconds(0) )
		         == std::future_status::ready );
	}

	void Get()
	{
		result_future.get();
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
	typedef std::unique_ptr<T> result_type;

	std::vector< std::unique_ptr<T> > results;
	std::mutex results_mut;
	std::condition_variable results_cond;
	std::atomic<bool> finished;
	std::atomic<bool> canceled;

	TaskResultControlBlock()
		: results()
		, results_mut()
		, results_cond()
		, finished(false)
		, canceled(false)
	{}

	template<class Func>
	void Run(Func& task_func)
	{
		if ( canceled )
			return;

		TaskFuncResult<T> fr = task_func();

		if ( fr.status == TaskStatus::Finished ||
		     fr.status == TaskStatus::Continuing ) {

			std::lock_guard<std::mutex> lock( results_mut );

			results.push_back( std::move(fr.ret) );

			if ( fr.status == TaskStatus::Finished )
				finished = true;

			results_cond.notify_all();

		} else if ( fr.status == TaskStatus::Canceled ) {

			std::lock_guard<std::mutex> lock( results_mut );

			canceled = true;
			results_cond.notify_all();
		}

		return;
	}

	void Cancel()
	{
		canceled = true;
		std::lock_guard<std::mutex> lock( results_mut );
		results_cond.notify_all();
	}

	bool IsFinished() const
	{
		return finished || canceled;
	}

	result_type Get()
	{
		std::unique_lock<std::mutex> lock( results_mut );

		results_cond.wait( lock, [=]() { return results.size() || finished || canceled; } );

		if ( !results.size() )
			return nullptr;

		std::unique_ptr<T> res = std::move( results[0] );
		results.erase( results.begin() );

		return res;
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
