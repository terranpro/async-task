//
//  ThreadExecutor.hpp - std::thread based executor implementation
//
//  Copyright (c) 2015 Brian Fransioli
//
//  Distributed under the Boost Software License, Version 1.0.
//  See accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt
//

#ifndef AS_THREAD_EXECUTOR_HPP
#define AS_THREAD_EXECUTOR_HPP

#include "Executor.hpp"

#include "Task.hpp"

#include <thread>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <chrono>
//#include <vector>
#include <algorithm>
#include <deque>

#include <cassert>

namespace as {

struct ThreadWork
{
	virtual ~ThreadWork() {}
	virtual bool operator()() = 0;
};

template<class Func>
struct ThreadWorkImpl
	: public ThreadWork
{
	Func func;

	ThreadWorkImpl(Func f)
		: func( std::move(f) )
	{}

	virtual bool operator()()
	{
		if ( func.Invoke() == TaskStatus::Finished )
			return true;
		return false;
	}
};

class ThreadExecutorImpl
{
	typedef std::chrono::high_resolution_clock Clock;
	typedef std::chrono::time_point<Clock> TimePoint;
	typedef std::chrono::milliseconds Interval;

	template<class T>
	struct JobQueue2
	{
		std::deque< std::unique_ptr<T> > que;

		T *Pop()
		{
			T *front = que.front().release();
			que.pop_front();
			return front;
		}

		void Push(T *obj)
		{
			que.emplace_back( obj );
		}

		size_t Count() const
		{
			return que.size();
		}

		bool Empty() const
		{
			return que.empty();
		}
	};

	std::atomic<bool> quit_requested;
	JobQueue2<ThreadWork> task_queue;
	std::mutex task_mut;
	std::condition_variable cond;
	Interval min_sleep_interval;
	std::thread thr;

public:
	ThreadExecutorImpl()
		: quit_requested(false)
		, task_queue()
		, task_mut()
		, cond()
		, min_sleep_interval(-1)
		, thr()
	{
		thr = std::thread( &ThreadExecutorImpl::ThreadEntryPoint, this );
	}

	~ThreadExecutorImpl()
	{
		{
			std::unique_lock<std::mutex> lock{ task_mut };
			quit_requested = true;
			cond.notify_all();
		}

		thr.join();

		assert( task_queue.Empty() );
	}

	void Schedule(Task task)
	{
		std::lock_guard<std::mutex> lock{ task_mut };
		//task_queue.push_back( std::move(info) );
		task_queue.Push( new ThreadWorkImpl<Task>{ std::move(task) } );
		cond.notify_all();
	}

	template<class Handler>
	void Schedule(TaskImplBase<Handler>&& ti)
	{
		std::lock_guard<std::mutex> lock{ task_mut };
		//task_queue.push_back( std::move(info) );
		task_queue.Push( new ThreadWorkImpl<TaskImplBase<Handler> >{ std::move(ti) } );
		cond.notify_all();
	}

	void ScheduleAfter(Task task, std::chrono::milliseconds time_ms)
	{
	}

	void Iteration()
	{
		// TODO
	}

	bool IsCurrent() const
	{
		return thr.get_id() == std::this_thread::get_id();
	}

private:
	void ThreadEntryPoint()
	{
		while( !quit_requested || !task_queue.Empty() )
			DoIteration();
	}

	void DoIteration()
	{
		auto lock = WaitForTasks();

		if ( task_queue.Empty() )
			return;

		DoProcessTasks( std::move(lock) );
	}

	std::unique_lock<std::mutex>
	WaitForTasks()
	{
		std::unique_lock<std::mutex> lock{ task_mut };

		cond.wait( lock,
		           [&]() {
			           return !task_queue.Empty() || quit_requested;
		           } );

		return lock;
	}

	void DoProcessTasks( std::unique_lock<std::mutex> lock )
	{
		auto job_count = task_queue.Count();

		while( std::unique_ptr<ThreadWork> tip{ task_queue.Pop() } ) {

			lock.unlock();

			auto fin = DoProcessTask( tip.get() );

			--job_count;

			if ( !job_count )
				return;

			lock.lock();

			if ( !fin )
				task_queue.Push( tip.release() );
		}
	}

	bool DoProcessTask( ThreadWork *tip )
	{
		return (*tip)();
	}
};

class ThreadExecutor
	: public Executor
{
	typedef std::chrono::high_resolution_clock Clock;
	typedef std::chrono::time_point<Clock> TimePoint;
	typedef std::chrono::milliseconds Interval;

	std::shared_ptr<ThreadExecutorImpl> impl;

public:
	ThreadExecutor()
		: impl( std::make_shared<ThreadExecutorImpl>() )
	{}

	ThreadExecutor(ThreadExecutor const&) = default;
	ThreadExecutor& operator=(ThreadExecutor const&) = default;

	void Schedule(Task task)
	{
		impl->Schedule(std::move(task));
	}

	template<class Handler>
	void Schedule(TaskImplBase<Handler>&& ti)
	{
		impl->Schedule(std::move(ti));
	}

	void ScheduleAfter(Task task, std::chrono::milliseconds time_ms)
	{
		impl->ScheduleAfter(std::move(task), time_ms);
	}

	void Iteration()
	{
		impl->Iteration();
	}

	bool IsCurrent() const
	{
		return impl->IsCurrent();
	}
};

Executor& Executor::GetDefault()
{
	static ThreadExecutor ex;

	return ex;
}

} // namespace as

#endif // AS_THREAD_EXECUTOR_HPP
