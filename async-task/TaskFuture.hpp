//
//  TaskFuture.hpp - Asynchronous Task Result Future
//
//  Copyright (c) 2015 Brian Fransioli
//
//  Distributed under the Boost Software License, Version 1.0.
//  See accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt
//

#ifndef AS_TASK_FUTURE_HPP
#define AS_TASK_FUTURE_HPP

#include "TaskControlBlock.hpp"

#include <memory>

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

} // namespace as

#endif // AS_TASK_FUTURE_HPP
