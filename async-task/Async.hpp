//
//  Async.hpp - Async dispatch of functions via executors
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

template<class ArgTuple, class Enable = void>
struct invoker_impl;

template<class Ex, class... Args>
struct invoker_impl<std::tuple<Ex, Args...>,
                    typename std::enable_if< is_executor< typename std::remove_reference<Ex>::type >::value >::type >
{
	typedef typename ChainResultOf<std::tuple<>,
	                               typename std::remove_reference<Args>::type...
	                              >::type result_type;

	typedef TaskFuture<result_type> future_type;

	template<class E, class... Funcs>
	static future_type async(E&& ex, Funcs&&... funcs)
	{
		auto r = std::make_shared<AsyncResult<result_type>>();

		auto c = build_chain( ex,
		                      std::forward<Funcs>(funcs)...,
		                      async_result_invocation<result_type>(r)
		                    );

		schedule( ex, AsyncTask<result_type, decltype(c)>( std::move(c), r ) );

		return {std::move(r)};
	}
};

template<class Func, class... Args>
struct invoker_impl<std::tuple<Func, Args...>,
                    typename std::enable_if< !is_executor< typename std::remove_reference<Func>::type >::value >::type >
{
	typedef typename ChainResultOf<std::tuple<>,
	                               typename std::remove_reference<Func>::type,
	                               typename std::remove_reference<Args>::type...
	                              >::type result_type;

	typedef TaskFuture<result_type> future_type;

	template<class... Funcs>
	static future_type async(Funcs&&... funcs)
	{
		using chain_type = invoker_impl< std::tuple<ThreadExecutor, Func, Args...> >;

		auto& ex = ThreadExecutor::GetDefault();

		return chain_type::async( ex, std::forward<Funcs>(funcs)... );
	}
};

template<class... Args>
struct invoker
	: invoker_impl< std::tuple<Args...> >
{};

template<class... Args>
auto async(Args&&... args)
	-> typename invoker<Args...>::future_type
{
	return invoker<Args...>::async( std::forward<Args>(args)... );
}

} // namespace as

#endif // AS_ASYNC_HPP
