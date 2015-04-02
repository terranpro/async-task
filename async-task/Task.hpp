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

class Task
{
private:
	std::shared_ptr<TaskImpl> impl;

	friend class ThreadExecutorImpl;

public:
	struct GenericTag {};
	struct CoroutineTag {};

public:
	Task() = default;

	Task(std::shared_ptr<TaskImpl> impl)
		: impl(impl)
	{}

	~Task() = default;

	Task(Task&&) = default;
	Task(Task const&) = default;

	Task& operator=(Task&&) = default;
	Task& operator=(Task const&) = default;

	void Invoke()
	{
		(*impl)();
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
