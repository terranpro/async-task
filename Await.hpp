#ifndef AS_AWAIT_HPP
#define AS_AWAIT_HPP

#include "Async.hpp"
#include "Executor.hpp"

namespace as {

template<class Result>
void await_schedule(Executor& context, Task task, TaskResult<Result> result)
{
	context.Schedule(task);

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
	auto tr_pair = as::make_task_pair( Task::CoroutineTag{},
	                                   std::forward<Func>(func),
	                                   std::forward<Args>(args)... );
	auto& task = tr_pair.first;
	auto& res  = tr_pair.second;

	await_schedule( context, task, res );

	return res.Get();
}

template<class Func, class... Args>
decltype( std::declval<Func>()(std::declval<Args>()...) )
await(Func&& func, Args&&... args)
{
	GlibExecutor ctxt;

	return await( ctxt,
	              std::forward<Func>(func),
	              std::forward<Args>(args)... );
}

} // namespace as

#endif // AS_AWAIT_HPP
