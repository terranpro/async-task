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

#include "TaskFuture.hpp"
#include "ThreadExecutor.hpp"

#include <atomic>
#include <mutex>
#include <utility>

#include <cassert>

namespace as {

/// Dispatch a callback in a thread context, i.e. an ExecutionContext
template<class Func, class... Args>
TaskFuture< decltype( std::declval<Func>()(std::declval<Args>()...) ) >
async(Executor& context, Func&& func, Args&&... args)
{
	auto timpl = make_task<TaskImplBase>( std::forward<Func>(func),
	                                      std::forward<Args>(args)... );
	context.Schedule( {timpl} );

	return { std::move(timpl)->GetControlBlock() };
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
	ThreadExecutor c;

	return async( c,
	              std::forward<Func>(func),
	              std::forward<Args>(args)... );
}

template<class Func>
void post(Executor& ex, Func&& func)
{
	auto impl = std::make_shared< TaskImplBase<void, PostInvoker<void> > >( std::forward<Func>(func) );

	ex.Schedule( { impl } );
}

} // namespace as

#endif // AS_ASYNC_HPP
