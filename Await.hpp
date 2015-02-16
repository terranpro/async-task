#ifndef AS_AWAIT_HPP
#define AS_AWAIT_HPP

#include "Async.hpp"
#include "ExecutionContext.hpp"

namespace as {

template<class Result>
void await_schedule(ExecutionContext& context, Task task, TaskResult<Result> result)
{
	context.AddTask(task);

	while( !task.IsFinished() ) {

		if ( detail::this_task_stack.size() ) {
			detail::this_task_stack[0]->Yield();
		}

		if ( context.IsCurrent() )
			context.Iteration();
	}

}

template<class Func, class... Args>
decltype( std::declval<Func>()(std::declval<Args>()...) )
await(ExecutionContext& context, Func&& func, Args&&... args)
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
	GlibExecutionContext ctxt;

	return await( ctxt,
	              std::forward<Func>(func),
	              std::forward<Args>(args)... );
}

} // namespace as

#endif // AS_AWAIT_HPP
