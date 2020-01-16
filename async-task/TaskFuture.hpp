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

#include "AsyncResult.hpp"

#include <memory>
#include <atomic>

namespace as {

template<class T>
class TaskFuture
{
	std::shared_ptr< AsyncResult<T> > result;

public:
	TaskFuture() = default;

	TaskFuture(std::shared_ptr<AsyncResult<T>> r)
		: result(std::move(r))
	{}

	T get()
	{
		return result->get();
	}

	void cancel()
	{
		result->cancel();
	}

	bool ready() const
	{
		return result->ready();
	}
};

} // namespace as

#endif // AS_TASK_FUTURE_HPP
