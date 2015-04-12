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
async(ThreadExecutor& context, Func&& func, Args&&... args)
{
	using Ret = decltype( std::declval<Func>()(std::declval<Args>()...) );

	context.Schedule( TaskImplBase<BaseInvoker<Ret> > (
		                  std::forward<Func>(func),
		                  std::forward<Args>(args)... ) );

	return {};
}

/// Dispatch a callback in thread; overload for shared_ptr context
template<class Func, class... Args>
TaskFuture< decltype( std::declval<Func>()(std::declval<Args>()...) ) >
async(std::shared_ptr<Executor> context, Func&& func, Args&&... args)
{
	return async( *context, std::forward<Func>(func), std::forward<Args>(args)... );
}

/// Dispatch a callback in a new thread context
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
void post(ThreadExecutor& ex, Func&& func)
{
	ex.Schedule( PostInvoker<ThreadExecutor,Func>( &ex, std::forward<Func>(func) ) );
}

template<class Func, class Func2>
void post(ThreadExecutor& ex, Func&& func, Func2&& func2)
{
	// invocation<Func> inv1( std::forward<Func>(func) );
	// invocation<Func2> inv2( std::forward<Func2>(func2) );
	// chain_invoke< decltype(inv1), decltype(inv2) > chain_inv;

	// post( ex, [chain_inv]() {
	// 		chain_inv.invoke();
	// } );

	//ex.Schedule( PostInvoker<ThreadExecutor,Func>( &ex, std::forward<Func>(func) ) );
}

} // namespace as

#endif // AS_ASYNC_HPP
