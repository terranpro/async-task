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

template<class Ex>
struct is_executor
	: std::false_type
{};

template<>
struct is_executor<ThreadExecutor>
	: std::true_type
{};

template<class Ex, class... Funcs>
auto async_impl(std::true_type, Ex& ex, Funcs&&... funcs)
	-> TaskFuture< typename ChainResultOf<std::tuple<>,
	                                      typename std::remove_reference<Funcs>::type...
	                                     >::type >
{
	using result_type = typename ChainResultOf<std::tuple<>,
	                                           typename std::remove_reference<Funcs>::type...
	                                          >::type;

	auto r = std::make_shared<AsyncResult<result_type>>();

	auto c = build_chain( ex,
	                      std::forward<Funcs>(funcs)...,
	                      async_result_invocation<result_type>(r)
	                    );

	schedule( ex, AsyncTask<result_type, decltype(c)>( std::move(c), r ) );

	return {std::move(r)};
}

/// Dispatch a callback in the default thread context
template<class Func, class... Args, class = typename std::enable_if< !is_executor<Func>::value >::type >
auto async_impl(std::false_type, Func&& func, Args&&... args)
	-> decltype( async_impl( std::true_type{},
	                         ThreadExecutor::GetDefault(),
	                         std::forward<Func>(func),
	                         std::forward<Args>(args)... ) )
{
	// ThreadExecutor c;

	return async_impl( std::true_type{},
	                   ThreadExecutor::GetDefault(),
	                   std::forward<Func>(func),
	                   std::forward<Args>(args)... );
}

template<class T, class... Args>
auto async(T&& t, Args&&... args)
	-> decltype( async_impl( typename is_executor<typename std::decay<T>::type>::type{}, std::forward<T>(t), std::forward<Args>(args)... ) )
{
	return async_impl( typename is_executor< typename std::decay<T>::type >::type{}, std::forward<T>(t), std::forward<Args>(args)... );
}

} // namespace as

#endif // AS_ASYNC_HPP
