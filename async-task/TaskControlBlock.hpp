//
//  TaskControlBlock.hpp - Task Control Block that holds functor,
//  synchronization, and status
//
//  Copyright (c) 2015 Brian Fransioli
//
//  Distributed under the Boost Software License, Version 1.0.
//  See accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt
//

#ifndef AS_TASK_CONTROL_BLOCK_HPP
#define AS_TASK_CONTROL_BLOCK_HPP

#include "TaskStatus.hpp"
#include "Channel.hpp"

#include <type_traits>

#include <cassert>

namespace as {

template<class Ret>
struct AsyncInvoker
{};

template<class Ret>
struct PostInvoker;

template<>
struct PostInvoker<void>
{
	typedef void result_type;
	typedef void reference_type;

	std::function<void()> taskfunc;

	template<class Func, class... Args>
	PostInvoker(Func&& func, Args&&... args)
		: taskfunc( std::bind( std::forward<Func>(func),
		                       std::forward<Args>(args)... ) )
	{}

	void operator()()
	{
		assert( taskfunc );

		taskfunc();
	}

	bool finished() const
	{
		return true;
	}
};


template<class Ret, class Enable = void>
struct TaskInvoker;

template<class Ret>
struct TaskInvoker<Ret,
         typename std::enable_if< !std::is_abstract<Ret>::value >::type
        >
{
	typedef Ret& result_type;
	typedef Ret& reference_type;

	std::function<Ret()> taskfunc;
	std::unique_ptr<Ret> result;

	TaskInvoker() = default;

	template<class Func>
	TaskInvoker( Func&& func )
		: taskfunc( std::forward<Func>(func) )
		, result()
	{}

	void operator()()
	{
		assert( taskfunc );

		result.reset( new Ret{ taskfunc() } );
	}

	result_type get() const
	{
		return *result;
	}

	bool finished() const
	{
		return result != nullptr;
	}
};

template<>
struct TaskInvoker<void>
{
	typedef void result_type;
	typedef void reference_type;

	std::function<void()> taskfunc;
	std::atomic<bool> is_set;

	TaskInvoker()
		: taskfunc()
		, is_set(false)
	{}

	template<class Func>
	TaskInvoker( Func&& func )
		: taskfunc( std::forward<Func>(func) )
		, is_set(false)
	{}

	void operator()()
	{
		taskfunc();

		is_set = true;
	}

	void get() const
	{}

	bool finished() const
	{
		return is_set;
	}
};

template< class T, class Invoker = TaskInvoker<T> >
struct TaskControlBlock
{
	typedef typename Invoker::result_type result_type;
	typedef typename Invoker::reference_type reference_type;

	Invoker invoker;
	std::mutex mut;
	std::condition_variable cond;

	TaskControlBlock()
		: invoker()
		, mut()
		, cond()
	{}

	template<class Func>
	explicit TaskControlBlock(Func&& func)
		: invoker( std::forward<Func>(func) )
		, mut()
		, cond()
	{}

	void Run()
	{
		if ( IsFinished() )
			return;

		invoker();

		cond.notify_all();
	}

	void Cancel()
	{
	}

	bool Valid() const
	{
		return true;
	}

	bool IsFinished() const
	{
		return Valid() == false || invoker.finished();
	}

	result_type Get()
	{
		std::unique_lock<std::mutex> lock{ mut };
		cond.wait( lock, [=]() { return invoker.finished(); } );
		return invoker.get();
	}

	void Wait()
	{
		std::unique_lock<std::mutex> lock{ mut };
		cond.wait( lock, [=]() { return invoker.finished(); } );
	}

	template<class Rep, class Period>
	WaitStatus WaitFor( std::chrono::duration<Rep,Period> const& dur ) const
	{
		std::unique_lock<std::mutex> lock{ mut };
		return cond.wait_for( lock, dur, [=]() { return invoker.finished(); } )
			? WaitStatus::Ready
			: WaitStatus::Timeout;
	}
};

template<class T>
struct TaskControlBlock< T, PostInvoker<T> >
{
	typedef PostInvoker<T> Invoker;

	typedef typename Invoker::result_type result_type;
	typedef typename Invoker::reference_type reference_type;

	Invoker invoker;
	bool finished;

	TaskControlBlock()
		: invoker()
		, finished(false)
	{}

	template<class Func>
	explicit TaskControlBlock(Func&& func)
		: invoker( std::forward<Func>(func) )
	{}

	void Run()
	{
		if ( IsFinished() )
			return;

		invoker();
		finished = true;
	}

	void Cancel()
	{}

	bool IsFinished() const
	{
		return finished;
	}

	result_type Get()
	{
		return;
	}
};

template<class T>
struct TaskControlBlock< TaskResult<T> >
{
	typedef typename Channel<T>::result_type result_type;

	Channel<T> channel;
	std::function< TaskResult<T>()> task_func;

	template<class Func>
	TaskControlBlock(Func&& tfunc)
		: channel()
		, task_func(std::forward<Func>(tfunc))
	{}

	void Run()
	{
		if ( !channel.IsOpen() )
			return;

		TaskResult<T> fr = task_func();

		if ( fr.status == TaskStatus::Finished ||
		     fr.status == TaskStatus::Continuing ) {

			channel.Put( std::move(fr) );

			if ( fr.status == TaskStatus::Finished )
				channel.Close();

		} else if ( fr.status == TaskStatus::Canceled ) {

			channel.Cancel();
		} else {
			// Do nothing - TaskStatus::Repeat implies not finished
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

} // namespace as

#endif // AS_TASK_CONTROL_BLOCK_HPP
