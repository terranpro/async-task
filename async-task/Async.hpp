//
//  Async.hpp - Async dispatch of functions via executors and locked objects
//
//  Copyright (c) 2015 Brian Fransioli
//
//  Distributed under the Boost Software License, Version 1.0.
//  See accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt
//

#ifndef AS_ASYNC_HPP
#define AS_ASYNC_HPP

#include "GlibExecutor.hpp"
#include "ThreadExecutor.hpp"

#include <atomic>
#include <mutex>
#include <utility>

#include <cassert>

namespace as {

/// Create a Task and deduced TaskResult<> pair
template<class TaskTag, class Func, class... Args>
std::pair<
         Task,
         TaskFuture< decltype( std::declval<Func>()(std::declval<Args>()...) ) >
        >
make_task_pair(TaskTag, Func&& func, Args&&... args)
{
	typedef decltype( std::declval<Func>()(std::declval<Args>()...) )
		result_type;

	auto timpl = make_task<TaskImpl>( std::forward<Func>(func),
	                                  std::forward<Args>(args)... );

	return std::make_pair( Task{ std::move(timpl) },
	                       TaskFuture<result_type>{ timpl->GetControlBlock() } );
}

/// Dispatch a callback in a thread context, i.e. an ExecutionContext
template<class Func, class... Args>
TaskFuture< decltype( std::declval<Func>()(std::declval<Args>()...) ) >
async(Executor& context, Func&& func, Args&&... args)
{
	auto tr_pair = make_task_pair( Task::GenericTag{},
	                               std::forward<Func>(func),
	                               std::forward<Args>(args)... );

	context.Schedule( tr_pair.first );

	return tr_pair.second;
}

/// Dispatch a callback in thread; overload for shared_ptr context
template<class Func, class... Args>
TaskFuture< decltype( std::declval<Func>()(std::declval<Args>()...) ) >
async(std::shared_ptr<Executor> context, Func&& func, Args&&... args)
{
	return async( *context, std::forward<Func>(func), std::forward<Args>(args)... );
}

/// Dispatch a callback in a new thread context (via GlibExecutionContext)
template<class Func, class... Args>
TaskFuture< decltype( std::declval<Func>()(std::declval<Args>()...) ) >
async(Func&& func, Args&&... args)
{
	// TODO: examine why launching 48+ GlibExecutor's might deadlock on
	// Cygwin; replaced default with ThreadExecutor's to start

	GlibExecutor c;
	//ThreadExecutor c;

	return async( c,
	              std::forward<Func>(func),
	              std::forward<Args>(args)... );
}

} // namespace as

#endif // AS_ASYNC_HPP
