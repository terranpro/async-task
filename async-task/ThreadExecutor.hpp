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
#include <vector>
#include <algorithm>

#include <cassert>

namespace as {

class ThreadExecutorImpl
{
	typedef std::chrono::high_resolution_clock Clock;
	typedef std::chrono::time_point<Clock> TimePoint;
	typedef std::chrono::milliseconds Interval;

	struct TaskInfo
	{
		Task task;
		TimePoint next_invocation;
		Interval interval_ms;

		TaskInfo(Task task)
			: task(task)
			, next_invocation()
			, interval_ms(0)
		{}
	};

	std::thread thr;
	std::mutex task_mut;
	std::vector<TaskInfo> task_queue;
	std::condition_variable cond;
	std::atomic<bool> quit_requested;
	Interval min_sleep_interval;

public:
	ThreadExecutorImpl()
		: thr()
		, task_mut()
		, task_queue()
		, cond()
		, quit_requested(false)
		, min_sleep_interval(-1)
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

		assert( task_queue.size() == 0 );
	}

	void Schedule(Task task)
	{
		TaskInfo info{task};

		std::lock_guard<std::mutex> lock{ task_mut };
		task_queue.push_back( std::move(info) );
		cond.notify_all();
	}

	void ScheduleAfter(Task task, std::chrono::milliseconds time_ms)
	{
		TaskInfo info{task};
		info.next_invocation = Clock::now() + time_ms;
		info.interval_ms = time_ms;

		std::lock_guard<std::mutex> lock{ task_mut };
		min_sleep_interval = std::min( min_sleep_interval, time_ms );

		task_queue.push_back( std::move(info) );
		cond.notify_all();
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
		while( !quit_requested ) {

			auto tasks = WaitForTasks();

			if ( tasks.size() == 0 )
				continue;

			auto next_tasks = ProcessTasks( std::move(tasks) );

			EnqueueRemaining( std::move(next_tasks) );
		}

		ProcessRemainingTasks();
	}

	std::vector<TaskInfo>
	WaitForTasks()
	{
		std::unique_lock<std::mutex> lock{ task_mut };

		auto cur_size = task_queue.size();
		auto wakeup_time = Clock::now() + min_sleep_interval;

		if ( min_sleep_interval > Interval(0) ) {
			cond.wait_for( lock, min_sleep_interval,
			               [&]() {
				               return ( task_queue.size() > cur_size
				                        || Clock::now() > wakeup_time
				                        || quit_requested );
			               } );
		} else {
			cond.wait( lock,
			           [&]() {
				           return task_queue.size() > 0 || quit_requested;
			           } );
		}

		if ( task_queue.size() == 0 )
			return {};

		return std::move( task_queue );
	}

	std::vector<TaskInfo>
	ProcessTasks( std::vector<TaskInfo>&& tasks ) const
	{
		std::vector<TaskInfo> next_tasks;

		for( auto& cur_info : tasks ) {
			if( cur_info.interval_ms > Interval(0) && Clock::now() < cur_info.next_invocation ) {
				next_tasks.push_back( cur_info );
				continue;
			}

			Task& cur_task = cur_info.task;
			cur_task.Invoke();
			if ( !cur_task.IsFinished() ) {

				cur_info.next_invocation = Clock::now() + cur_info.interval_ms;

				next_tasks.push_back( std::move(cur_info) );
			}

		}

		return next_tasks;
	}

	void EnqueueRemaining( std::vector<TaskInfo>&& next_tasks )
	{
		std::unique_lock<std::mutex> lock{ task_mut };

		task_queue.insert( task_queue.end(), next_tasks.begin(), next_tasks.end() );

		auto min_interval_it =
			std::min_element( task_queue.begin(),
			                  task_queue.end(),
			                  [](const TaskInfo& t1,
			                     const TaskInfo& t2)
			                  {
				                  return t1.interval_ms < t2.interval_ms;
			                  }
			                );

		if ( min_interval_it == task_queue.end() )
			return;

		min_sleep_interval = min_interval_it->interval_ms;
	}

	void ProcessRemainingTasks()
	{
		// TODO: refactor this nicely
		// before exiting the thread, run any tasks left in queue
		std::vector<TaskInfo> tasks;

		for ( std::unique_lock<std::mutex> lock{ task_mut };
		      task_queue.size() != 0 || tasks.size() != 0;
		    )
		{
			if ( task_queue.size() )
				std::move( task_queue.begin(), task_queue.end(), std::back_inserter(tasks) );
			task_queue.clear();

			assert( task_queue.size() == 0 );

			lock.unlock();

			tasks = ProcessTasks( std::move(tasks) );

			lock.lock();
		}
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

} // namespace as

#endif // AS_THREAD_EXECUTOR_HPP
