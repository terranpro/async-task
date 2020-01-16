//
//  Await.hpp - Coroutine based await functionality
//
//  Copyright (c) 2015 Brian Fransioli
//
//  Distributed under the Boost Software License, Version 1.0.
//  See accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt
//

#ifndef AS_AWAIT_HPP
#define AS_AWAIT_HPP

#include "Async.hpp"
#include "Executor.hpp"
#include "CoroutineTaskImpl.hpp"

namespace as {

// TODO: you can do this!! fighting brian~~* :D
template<class Ex, class Func, class... Args>
TaskFuture< decltype( std::declval<Func>()(std::declval<Args>()...) ) >
await(Ex& ex, Func&& func, Args&&... args)
{
	typedef decltype( std::declval<Func>()(std::declval<Args>()...) ) result_type;

	auto bound = std::bind( std::forward<Func>(func), std::forward<Args>(args)... );
	// invocation<decltype(bound)> inv( std::move(bound) );

	auto r = std::make_shared<AsyncResult<result_type>>();

	auto c = build_chain( ex, std::move(bound), async_result_invocation<result_type>(r) );

	typedef CoroutineTaskImpl< decltype(c) > coro_task_type;

	coro_task_type ct{ std::move(c) };

	ex.schedule( std::move(ct) );

	return{ std::move(r) };
}

template<class Func, class... Args>
TaskFuture< decltype( std::declval<Func>()(std::declval<Args>()...) ) >
await(Func&& func, Args&&... args)
{
	//GlibExecutor ctxt;
	auto ctxt = ThreadExecutor::GetDefault();

	return await( ctxt,
	              std::forward<Func>(func),
	              std::forward<Args>(args)... );
}

#define AWAIT( fut ) \
	do {									\
		as::this_task::yield(); \
		std::this_thread::sleep_for( std::chrono::microseconds(1) ); \
	} while ( !fut.ready() )

} // namespace as

#endif // AS_AWAIT_HPP
