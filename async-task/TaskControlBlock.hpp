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

template<class Ret, class Enable = void>
struct TaskInvoker;

template<class Ret>
struct TaskInvoker<Ret,
         typename std::enable_if< !std::is_abstract<Ret>::value >::type
        >
{
	typedef Ret& result_type;
	typedef Ret& reference_type;

	std::unique_ptr<Ret> result;

	TaskInvoker() = default;

	template<class Func, class Promise>
	void RunHelper( Func&& task_func, Promise& promise )
	{
		assert( task_func );

		result.reset( new Ret{ task_func() } );
		promise.set_value( *result );
	}

	bool IsSet() const
	{
		return result != nullptr;
	}
};

template<>
struct TaskInvoker<void>
{
	typedef void result_type;
	typedef void reference_type;

	std::atomic<bool> is_set;

	TaskInvoker()
		: is_set(false)
	{}

	template<class Func, class Promise>
	void RunHelper( Func&& task_func, Promise& promise )
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
struct TaskControlBlock
	: public TaskInvoker<T>
{
	typedef TaskInvoker<T> base_type;

	typedef typename base_type::result_type result_type;
	typedef typename base_type::reference_type reference_type;

	std::promise<reference_type> result_promise;
	std::shared_future<reference_type> result_future;

	TaskControlBlock()
		: result_promise()
		, result_future( result_promise.get_future() )
	{}

	template<class Func>
	explicit TaskControlBlock(Func&& func)
		: TaskInvoker<T>( std::forward<Func>(func) )
		, result_promise()
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
struct TaskControlBlock< TaskResult<T> >
{
	typedef typename Channel<T>::result_type result_type;

	Channel<T> channel;

	TaskControlBlock() = default;

	template<class Func>
	void Run(Func& task_func)
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
