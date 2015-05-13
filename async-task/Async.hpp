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

template<class Ex, class Func, class... Args>
void post(Ex& ex, Func&& func, Args&&... args)
{
	// using sc = SplitBy< IsCallable, Func, Args... >;
	// using ib = invoker_builder< typename sc::true_types, typename sc::false_types >;

	// auto c = ib::build( std::forward<Func>(func), std::forward<Args>(args)... );

	std::cout << typeid( typename ChainResultOf<std::tuple<void>, Func, Args... >::type ).name() << "\n";

	auto c = build_chain( ex, std::forward<Func>(func), std::forward<Args>(args)... );

	schedule( ex, PostTask<Ex,decltype(c)>( &ex, std::move(c) ) );
	//ex.schedule( PostTask<Ex,decltype(c)>( &ex, std::move(c) ) );
}

/// Dispatch a callback in a thread context, i.e. an ExecutionContext
template<class Ex, class Func>
auto async(Ex& ex, Func&& func)
	-> TaskFuture< typename ChainResultOf<std::tuple<>, typename std::remove_reference<Func>::type >::type >
{
	schedule( ex, PostTask<Ex,Func>( &ex, std::forward<Func>(func) ) );
	return {};
}

template<class Ex, class Func, class... Args>
auto async(Ex& ex, Func&& func, Args&&... args)
	-> TaskFuture< typename ChainResultOf<std::tuple<>,
	                                      typename std::remove_reference<Func>::type,
	                                      typename std::remove_reference<Args>::type... >::type >
{
	//std::cout << typeid( typename ChainResultOf<std::tuple<>, Func, Args... >::type ).name() << "\n";

	auto c = build_chain( ex, std::forward<Func>(func), std::forward<Args>(args)... );

	schedule( ex, PostTask<Ex,decltype(c)>( &ex, std::move(c) ) );

	return {};
}

/// Dispatch a callback in a new thread context
// template<class Func, class... Args>
// TaskFuture< decltype( std::declval<Func>()(std::declval<Args>()...) ) >
// async(Func&& func, Args&&... args)
// {
// 	ThreadExecutor c;

// 	return async( c,
// 	              std::forward<Func>(func),
// 	              std::forward<Args>(args)... );
// }

} // namespace as

#endif // AS_ASYNC_HPP
