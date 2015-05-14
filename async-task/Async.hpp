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

/// Dispatch a callback in a thread context, i.e. an ExecutionContext

template<class R>
std::function<void(R)>
create_async_functor(std::shared_ptr<AsyncResult<R>> ar, std::false_type)
{
	return [ar](R r) {
		ar->set( std::move(r) );
	       };
}

std::function<void()>
create_async_functor(std::shared_ptr<AsyncResult<void>> ar, std::true_type)
{
	return [ar]() {
		ar->set();
	       };
}

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
	                      create_async_functor(r, typename std::is_void<result_type>::type{} )
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
	                      [r](result_type haha) {
		                      r->set(haha);
	                      }
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
