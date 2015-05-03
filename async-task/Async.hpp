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
#include "TaskImpl.hpp"

#include <atomic>
#include <mutex>
#include <utility>

#include <cassert>

namespace as {

template<class Ex, class Func>
void post(Ex& ex, Func&& func)
{
	ex.schedule( PostTask<Ex,Func>( &ex, std::forward<Func>(func) ) );
}

template<class Ex, class Func, class... ThenFuncs>
void post(Ex& ex, Func&& func, ThenFuncs&&... funcs)
{
	// using inv1_type = invocation<Func>;

	// using inv1_result_type = typename inv1_type::result_type;

	// using chain_type = chain_invocation< inv1_type, invocation<ThenFuncs>... >;

	// chain_type chain_inv( inv1_type( std::forward<Func>(func) ),
	//                       std::forward<ThenFuncs>(funcs)... );

	// post( ex, [=]() mutable {
	// 		chain_inv.invoke();
	// } );

	using sc = SplitBy< IsCallable, Func, ThenFuncs... >;
	using ib = invoker_builder< typename sc::true_types, typename sc::false_types >;
	ib b;

	auto c = b.build( std::forward<Func>(func), std::forward<ThenFuncs>(funcs)... );

	// std::cout << typeid(typename sc::true_types).name() << "\n";

	post( ex, [c]() mutable {
			c.invoke();
		} );
}

/// Dispatch a callback in a thread context, i.e. an ExecutionContext
template<class Ex, class Func, class... Args>
TaskFuture< void >
async(Ex& ex, Func&& func, Args&&... args)
{
	// using Ret = decltype( std::declval<Func>()(std::declval<Args>()...) );

	// ex.schedule( TaskImplBase<BaseInvoker<Ret> > (
	// 	                  std::forward<Func>(func),
	// 	                  std::forward<Args>(args)... ) );

	using sc = SplitBy< IsCallable, Func, Args... >;
	using ib = invoker_builder< typename sc::true_types, typename sc::false_types >;
	ib b;

	auto c = b.build( std::forward<Func>(func), std::forward<Args>(args)... );

	// std::cout << typeid(typename sc::true_types).name() << "\n";

	auto ff = [c]() mutable {
		c.invoke();
	};

	ex.schedule( PostTask<Ex, decltype(ff)>( &ex, std::move(ff) ) );

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

} // namespace as

#endif // AS_ASYNC_HPP
