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

void await_schedule(Executor& context, Task task)
{
	context.Schedule( std::move(task) );

	while( !task.IsFinished() ) {

#ifdef AS_USE_COROUTINE_TASKS
		if ( detail::this_task_stack.size() ) {
			detail::this_task_stack[0]->Yield();
		}
#endif // AS_USE_COROUTINE_TASKS

		if ( context.IsCurrent() )
			context.Iteration();
	}

}

template<class Func, class... Args>
decltype( std::declval<Func>()(std::declval<Args>()...) )
await(Executor& context, Func&& func, Args&&... args)
{
	auto bound = std::bind( std::forward<Func>(func), std::forward<Args>(args)... );
	invocation<decltype(bound)> inv( std::move(bound) );

	CoroutineTaskImpl< decltype(inv) > ct{ std::move(inv) };

	Task task{ true, std::move(ct) };

	await_schedule( context, std::move(task) );
}

template<class Func, class... Args>
decltype( std::declval<Func>()(std::declval<Args>()...) )
await(Func&& func, Args&&... args)
{
	//GlibExecutor ctxt;
	auto ctxt = ThreadExecutor::GetDefault();

	return await( ctxt,
	              std::forward<Func>(func),
	              std::forward<Args>(args)... );
}

} // namespace as

#endif // AS_AWAIT_HPP
