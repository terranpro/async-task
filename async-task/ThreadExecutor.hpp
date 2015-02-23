//
//  bind.hpp - binds function objects to arguments
//
//  Copyright (c) 2015 Brian Fransioli
//
//  Distributed under the Boost Software License, Version 1.0.
//  See accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt
//

#ifndef AS_THREAD_EXECUTOR_HPP
#define AS_THREAD_EXECUTOR_HPP

#include "Task.hpp"

#include <thread>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <chrono>
#include <vector>
#include <algorithm>

namespace as {

class ThreadExecutor
	: public Executor
{
	typedef std::chrono::high_resolution_clock Clock;
	typedef std::chrono::time_point<Clock> TimePoint;
	typedef std::chrono::milliseconds Interval;

	struct TaskInfo
	{
		Task task;
		TimePoint next_invocation{};
		Interval interval_ms{0};

		TaskInfo(Task task)
			: task(task)
		{}
	};

	std::thread thr;
	std::mutex task_mut;
	std::vector<TaskInfo> task_queue;
	std::condition_variable cond;
	std::atomic<bool> quit_requested;
	Interval min_sleep_interval;

public:
	ThreadExecutor()
		: thr( &ThreadExecutor::ThreadEntryPoint, this )
		, task_mut()
		, task_queue()
		, cond()
		, quit_requested(false)
		, min_sleep_interval(-1)
	{}

	~ThreadExecutor()
	{
		quit_requested = true;

		{
			std::unique_lock<std::mutex> lock{ task_mut };
			cond.notify_all();
		}

		thr.join();
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

	bool IsCurrent()
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

			auto next_tasks = ProcessTasks( tasks );

			EnqueueRemaining( std::move(next_tasks) );
		}
	}

	std::vector<TaskInfo>
	WaitForTasks()
	{
		auto wakeup_condition = [&]() {
			return task_queue.size() > 0 || quit_requested;
		};

		std::unique_lock<std::mutex> lock{ task_mut };

		if ( min_sleep_interval > Interval(0) )
			cond.wait_for( lock, min_sleep_interval, wakeup_condition );
		else
			cond.wait( lock, wakeup_condition );

		if ( task_queue.size() == 0 )
			return {};

		return std::move( task_queue );
	}

	std::vector<TaskInfo>
	ProcessTasks( std::vector<TaskInfo> tasks ) const
	{
		decltype(tasks) next_tasks;

		for( auto& cur_info : tasks ) {
			if( cur_info.interval_ms > Interval(0) && Clock::now() < cur_info.next_invocation ) {
				next_tasks.push_back( cur_info );
				continue;
			}

			Task cur_task = cur_info.task;
			cur_task.Invoke();
			if ( !cur_task.IsFinished() ) {

				cur_info.next_invocation = Clock::now() + cur_info.interval_ms;

				next_tasks.push_back( cur_info );
			}

		}

		return next_tasks;
	}

	void EnqueueRemaining( std::vector<TaskInfo> next_tasks )
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
};

} // namespace as

#endif // AS_THREAD_EXECUTOR_HPP
