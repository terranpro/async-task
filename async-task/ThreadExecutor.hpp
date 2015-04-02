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
		TaskInfo *next;

		TaskInfo(Task&& task)
			: task( std::move(task) )
			, next_invocation()
			, interval_ms(0)
			, next(nullptr)
		{}

	};

	class TaskInfoQueue
	{
		TaskInfo *head;
		TaskInfo *tail;
		size_t count;

	public:
		TaskInfoQueue()
			: head(nullptr)
			, tail(nullptr)
			, count(0)
		{}

		~TaskInfoQueue()
		{
			while( auto tip = Pop() )
				delete tip;
		}

		TaskInfoQueue(TaskInfoQueue&& other)
			: head( other.head )
			, tail( other.tail )
			, count( other.count )
		{
			other.head = nullptr;
			other.tail = nullptr;
			other.count = 0;
		}

		TaskInfoQueue(TaskInfoQueue const&) = delete;
		TaskInfoQueue& operator=(TaskInfoQueue const&) = delete;

	public:
		TaskInfo *Pop()
		{
			if ( head ) {
				auto tip = head;
				if ( head == tail )
					tail = head->next;
				head = head->next;

				tip->next = nullptr;

				--count;

				return tip;
			}

			return nullptr;
		}

		void Push(TaskInfo&& ti)
		{
			auto tip = new TaskInfo( std::move(ti) );
			Push(tip);
		}

		void Push(TaskInfo *tip)
		{
			if ( !tail ) {
				head = tail = tip;
			} else {
				tail->next = tip;
				tail = tip;
			}

			++count;
		}

		bool Empty() const
		{
			return head == tail && tail == nullptr;
		}

		size_t Count() const
		{
			return count;
		}
	};

	struct Context
	{

	};

	std::atomic<bool> quit_requested;
	TaskInfoQueue task_queue;
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
		TaskInfo info{ std::move(task) };

		std::lock_guard<std::mutex> lock{ task_mut };
		//task_queue.push_back( std::move(info) );
		task_queue.Push( std::move(info) );
		cond.notify_all();
	}

	void ScheduleAfter(Task task, std::chrono::milliseconds time_ms)
	{
		TaskInfo info{ std::move(task) };
		info.next_invocation = Clock::now() + time_ms;
		info.interval_ms = time_ms;

		std::lock_guard<std::mutex> lock{ task_mut };
		min_sleep_interval = std::min( min_sleep_interval, time_ms );

		//task_queue.push_back( std::move(info) );
		task_queue.Push( std::move(info) );
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

		//auto cur_size = task_queue.size();
		auto wakeup_time = Clock::now() + min_sleep_interval;

		if ( min_sleep_interval > Interval(0) ) {
			cond.wait_for( lock, min_sleep_interval,
			               [&]() {
				               return ( // task_queue.size() > cur_size ||
				                        Clock::now() > wakeup_time
				                        || quit_requested );
			               } );
		} else {
			cond.wait( lock,
			           [&]() {
				           return !task_queue.Empty() || quit_requested;
			           } );
		}

		return lock;
	}

	void DoProcessTasks( std::unique_lock<std::mutex> lock )
	{
		auto job_count = task_queue.Count();

		while( std::unique_ptr<TaskInfo> tip{ task_queue.Pop() } ) {

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

	bool DoProcessTask( TaskInfo *tip )
	{
		if( tip->interval_ms > Interval(0) && Clock::now() < tip->next_invocation ) {
			return false;
		}

		auto& cur_task = tip->task;
		cur_task.Invoke();
		if ( !cur_task.IsFinished() ) {

			tip->next_invocation = Clock::now() + tip->interval_ms;

			return false;
		}

		return true;
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

Executor& Executor::GetDefault()
{
	static ThreadExecutor ex;

	return ex;
}

} // namespace as

#endif // AS_THREAD_EXECUTOR_HPP
