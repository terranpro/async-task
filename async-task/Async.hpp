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
	auto c = build_chain( ex, std::forward<Func>(func), std::forward<Args>(args)... );

	schedule( ex, PostTask<Ex,decltype(c)>( &ex, std::move(c) ) );
}

template<class R>
struct async_result_invocation
{
	std::shared_ptr<AsyncResult<R>> ar;

	async_result_invocation(std::shared_ptr<AsyncResult<R>>& ar)
		: ar(ar)
	{}

	void operator()( R r )
	{
		ar->set( std::move(r) );
	}
};

template<>
struct async_result_invocation<void>
{
	std::shared_ptr<AsyncResult<void>> ar;

	async_result_invocation(std::shared_ptr<AsyncResult<void>>& ar)
		: ar(ar)
	{}

	void operator()()
	{
		ar->set();
	}
};

template<class Ex, class Func>
auto async(Ex& ex, Func&& func)
	-> TaskFuture< typename ChainResultOf<std::tuple<>, typename std::remove_reference<Func>::type >::type >
{
	using result_type = typename ChainResultOf<std::tuple<>,
	                                           typename std::remove_reference<Func>::type
	                                          >::type;

	auto r = std::make_shared<AsyncResult<result_type>>();

	auto c = build_chain( ex,
	                      std::forward<Func>(func),
	                      async_result_invocation<result_type>(r)
	                    );

	schedule( ex, AsyncTask<result_type, Ex,decltype(c)>( &ex, std::move(c), r ) );

	return TaskFuture<result_type>(std::move(r));
}

template<class Ex, class Func, class... Args>
auto async(Ex& ex, Func&& func, Args&&... args)
	-> TaskFuture< typename ChainResultOf<std::tuple<>,
	                                      typename std::remove_reference<Func>::type,
	                                      typename std::remove_reference<Args>::type... >::type >
{
	using result_type = typename ChainResultOf<std::tuple<>,
	                                           typename std::remove_reference<Func>::type,
	                                           typename std::remove_reference<Args>::type... >::type;

	auto r = std::make_shared<AsyncResult<result_type>>();

	auto c = build_chain( ex,
	                      std::forward<Func>(func),
	                      std::forward<Args>(args)...,
	                      async_result_invocation<result_type>(r)
	                    );

	schedule( ex, AsyncTask<result_type, Ex,decltype(c)>( &ex, std::move(c), r ) );

	return {std::move(r)};
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
