//
//  Executor.hpp - Executor interface abstraction
//
//  Copyright (c) 2015 Brian Fransioli
//
//  Distributed under the Boost Software License, Version 1.0.
//  See accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt
//

#ifndef AS_EXECUTOR_HPP
#define AS_EXECUTOR_HPP

#include "Task.hpp"

#include <functional>
#include <memory>
#include <chrono>

namespace as {

class Executor
{
public:
	virtual ~Executor() {}

	virtual void Schedule(Task task) = 0;
	virtual void ScheduleAfter(Task task, std::chrono::milliseconds time_ms) = 0;

	virtual void Iteration() = 0;
	virtual bool IsCurrent() const = 0;

	// TODO: can this be done...
	// static Executor& GetCurrent()
	// {

	// }
};

} // namespace as

#endif // AS_EXECUTOR_HPP
