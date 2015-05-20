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
#include <typeinfo>

namespace as {

class Executor
{
public:
	virtual ~Executor() {}

	virtual void Schedule(Task task) = 0;
	virtual void ScheduleAfter(Task task, std::chrono::milliseconds time_ms) = 0;

	virtual void Iteration() = 0;
	virtual bool IsCurrent() const = 0;

	virtual void Run() = 0;
	virtual void Shutdown() = 0;

	static Executor& GetDefault();

	const std::type_info& Type() const;
  template <class ExecutorType> ExecutorType* Target();
  template <class ExecutorType> const ExecutorType* Target() const;

	template<class Handler>
	void schedule(Handler&& handler);
};

template<class Ex>
struct is_executor
	: std::false_type
{};

template<>
struct is_executor<Executor>
	: std::true_type
{};

template<class Handler>
void Executor::schedule(Handler&& handler)
{
	this->Schedule( Task{ true, std::forward<Handler>(handler) } );
}


} // namespace as

#endif // AS_EXECUTOR_HPP
