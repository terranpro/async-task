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

#include "TaskImpl.hpp"
#include "TaskControlBlock.hpp"
#include "TaskStatus.hpp"
#include "Channel.hpp"

#ifdef AS_USE_COROUTINE_TASKS
#include "CoroutineTaskImpl.hpp"
#endif // AS_USE_COROUTINE_TASKS

namespace as {

template<class T>
class TaskFuture
{
	std::shared_ptr< TaskControlBlock<T> > ctrl;

public:
	TaskFuture() = default;

	TaskFuture( std::shared_ptr<TaskControlBlock<T> > ctrl)
		: ctrl(ctrl)
	{}

	typename TaskControlBlock<T>::result_type
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
	std::shared_ptr<TaskImplBase> impl;

	friend class ThreadExecutorImpl;

public:
	struct GenericTag {};
	struct CoroutineTag {};

public:
	Task() = default;

	Task(std::shared_ptr<TaskImplBase> impl)
		: impl(impl)
	{}

	~Task() = default;

	Task(Task&&) = default;
	Task(Task const&) = default;

	Task& operator=(Task&&) = default;
	Task& operator=(Task const&) = default;

	void Invoke()
	{
		impl->Invoke();
	}

	void Yield()
	{
		impl->Yield();
	}

	bool IsFinished() const
	{
		return impl->IsFinished();
	}

	void Cancel()
	{
		impl->Cancel();
	}
};

} // namespace as

#endif // AS_TASK_HPP
