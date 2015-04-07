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
#include "ThreadRegistry.hpp"

#include <thread>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <chrono>
#include <algorithm>
#include <deque>

#include <iostream>

#include <boost/pool/pool_alloc.hpp>

#include <cassert>

namespace as {

struct ThreadWork
{
	ThreadWork *next;

	ThreadWork()
		: next(nullptr)
	{}

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
	struct JobQueue
	{
		std::deque< std::unique_ptr<T> > que;
		//std::vector< std::unique_ptr<T> > que;

		const T* Front() const
		{
			return que.front().get();
		}

		T *Pop()
		{
			T *front = que.front().release();
			que.pop_front();
			//que.erase( que.begin() );

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

	template<class T>
	struct IntrusiveJobQueue
	{
		T *head;
		T *tail;
		size_t count;

		IntrusiveJobQueue()
			: head(nullptr)
			, tail(nullptr)
			, count(0)
		{}

		IntrusiveJobQueue(IntrusiveJobQueue&& other)
			: head( other.head )
			, tail( other.tail )
			, count( other.count )
		{
			other.head = nullptr;
			other.tail = nullptr;
			other.count = 0;
		}

		const T* Front() const
		{
			assert( head );

			return head;
		}

		T *Pop()
		{
			assert( head );

			auto tmp = head;

			if ( head == tail )
				head = tail = nullptr;
			else
				head = head->next;

			--count;

			return tmp;
		}

		void Push(T *obj)
		{
			++count;

			if ( !head ) {
				head = tail = obj;
				return;
			}

			tail->next = obj;
			tail = obj;
			obj->next = nullptr;
		}

		size_t Count() const
		{
			return count;
		}

		bool Empty() const
		{
			return head == nullptr;
		}
	};

	struct Context
	{
		Registry<ThreadExecutorImpl, Context> registry;
		JobQueue<ThreadWork> priv_task_queue;
		ThreadExecutorImpl *ex;

		Context(ThreadExecutorImpl *ex)
			: registry( ex, this )
			, priv_task_queue()
			, ex(ex)
		{}
	};

	IntrusiveJobQueue<ThreadWork> task_queue;
	std::mutex task_mut;
	std::condition_variable cond;
	std::atomic<bool> quit_requested;
	std::thread thr;

public:
	ThreadExecutorImpl()
		: task_queue()
		, task_mut()
		, cond()
		, quit_requested(false)
		, thr()
	{
		thr = std::thread( &ThreadExecutorImpl::ThreadEntryPoint, this );
	}

	ThreadExecutorImpl(std::string)
		: task_queue()
		, task_mut()
		, cond()
		, quit_requested(false)
		, thr()
	{}

	~ThreadExecutorImpl()
	{
		{
			std::unique_lock<std::mutex> lock{ task_mut };
			quit_requested = true;
			cond.notify_all();
		}

		if ( thr.joinable() )
			thr.join();

		assert( task_queue.Empty() );
	}

	void Schedule(Task task)
	{
		auto tw = new ThreadWorkImpl<Task>{ std::move(task) };

		std::lock_guard<std::mutex> lock{ task_mut };
		//task_queue.push_back( std::move(info) );
		task_queue.Push( tw );

		if ( task_queue.Front() == tw )
			cond.notify_one();
	}

	template<class Handler>
	void Schedule(Handler&& ti)
	{
		auto tw = new ThreadWorkImpl<Handler>{ std::move(ti) };

		std::lock_guard<std::mutex> lock{ task_mut };
		//task_queue.push_back( std::move(info) );
		task_queue.Push( tw );

		// if ( thr.joinable() == false || IsCurrent() )
		// 	return;

		// if ( task_queue.Front() == tw )
		// 	cond.notify_one();
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

	void Run()
	{
		while( !task_queue.Empty() )
			DoIteration();
	}

	void Shutdown()
	{
		{
			std::unique_lock<std::mutex> lock{ task_mut };
			assert( thr.joinable() );

			quit_requested = true;
		}

		thr.join();
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
		IntrusiveJobQueue<ThreadWork> jobs = std::move(task_queue);

		lock.unlock();

		auto job_count = jobs.Count();

		while( job_count ) {
			std::unique_ptr<ThreadWork> tip{ jobs.Pop() };

			auto fin = DoProcessTask( tip.get() );

			if ( !fin ) {
				lock.lock();
				task_queue.Push( tip.release() );
				lock.unlock();
			}

			--job_count;
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

	ThreadExecutor(std::string)
		: impl( std::make_shared<ThreadExecutorImpl>("haha") )
	{}

	ThreadExecutor(ThreadExecutor const&) = default;
	ThreadExecutor& operator=(ThreadExecutor const&) = default;

	void Schedule(Task task)
	{
		impl->Schedule(std::move(task));
	}

	template<class Handler>
	void Schedule(Handler&& ti)
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

	void Run()
	{
		return impl->Run();
	}

	void Shutdown()
	{
		return impl->Shutdown();
	}
};

Executor& Executor::GetDefault()
{
	static ThreadExecutor ex;

	return ex;
}

} // namespace as

#endif // AS_THREAD_EXECUTOR_HPP
