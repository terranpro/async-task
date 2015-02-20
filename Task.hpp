#ifndef AS_TASK_HPP
#define AS_TASK_HPP

#include <memory>
#include <future>
#include <vector>
#include <functional>

// TODO: enable when platform boost supports context
//#undef AS_USE_COROUTINE_TASKS

#include "TaskContext.hpp"
#include "TaskFunction.hpp"

#ifdef AS_USE_COROUTINE_TASKS
#include "CoroutineTaskContext.hpp"
#endif // AS_USE_COROUTINE_TASKS

namespace as {

template<class T>
struct TaskResultControlBlock
{
	std::unique_ptr<T> result;
	std::promise<T&> result_promise;
	std::shared_future<T&> result_future{ result_promise.get_future() };
};

template<>
struct TaskResultControlBlock<void>
{
	std::promise<void> result_promise;
	std::shared_future<void> result_future{ result_promise.get_future() };
};

template<class T>
class TaskResult
{
	std::shared_ptr< TaskResultControlBlock<T> > ctrl;

	template<class U> friend struct TaskFunction;

private:
	TaskResult( std::shared_ptr<TaskResultControlBlock<T> > ctrl)
		: ctrl(ctrl)
	{}

public:
	enum Status {
		Deferred = static_cast<int>( std::future_status::deferred ),
		Ready = static_cast<int>( std::future_status::ready ),
		Timeout = static_cast<int>( std::future_status::timeout )
	};

public:
	TaskResult()
		: ctrl()
	{}

public:
	T& Get()
	{
		return ctrl->result_future.get();
	}

	bool Valid() const
	{
		return ctrl->result_future.valid();
	}

	void Wait() const
	{
		return ctrl->result_future.wait();
	}

	template<class Rep, class Period>
	Status WaitFor( std::chrono::duration<Rep,Period> const& dur ) const
	{
		return static_cast<Status>( static_cast<int>( ctrl->result_future.wait_for( dur ) ) );
	}
};

template<>
class TaskResult<void>
{
	std::shared_ptr< TaskResultControlBlock<void> > ctrl;

	template<class U> friend struct TaskFunction;

private:
	TaskResult(std::shared_ptr< TaskResultControlBlock<void> > ctrl)
		: ctrl(ctrl)
	{}

public:
	void Get()
	{
		ctrl->result_future.get();
	}

	bool Valid()
	{
		return ctrl->result_future.valid();
	}
};

template<class Ret>
struct TaskInvokerRun
{
	void operator()(TaskResultControlBlock<Ret>& ctrl)
	{

	}
};

template<class Ret>
struct TaskFunction
	: public TaskFunctionBase
{
	std::shared_ptr< TaskResultControlBlock<Ret> > ctrl;
	std::function<Ret()> task_func;

	template<class Func, class... Args>
	TaskFunction( Func&& func, Args&&... args )
		: ctrl( CreateControlBlock() )
		, task_func( std::bind(std::forward<Func>(func), std::forward<Args>(args)... ) )
	{}

	void Run()
	{
		if ( !ctrl )
			ctrl = CreateControlBlock();

		ctrl->result.reset( new Ret{ task_func() } );

		ctrl->result_promise.set_value( *ctrl->result );

		// Reset the local copy of the control block to indicate finished
		ctrl.reset();
	}

	bool IsFinished() const
	{
		return !ctrl;
	}

	TaskResult<Ret> GetResult()
	{
		return { ctrl };
	}

private:
	std::shared_ptr< TaskResultControlBlock<Ret> > CreateControlBlock() const
	{
		return std::make_shared< TaskResultControlBlock<Ret> >();
	}
};

template<>
struct TaskFunction<void>
	: public TaskFunctionBase
{
	std::shared_ptr< TaskResultControlBlock<void> > ctrl;
	std::function<void()> task_func;

	template<class Func, class... Args>
	TaskFunction( Func&& func, Args&&... args )
		: ctrl( CreateControlBlock() )
		, task_func( std::bind( std::forward<Func>(func), std::forward<Args>(args)... ) )
	{}

	void Run()
	{
		if ( !ctrl )
			ctrl = CreateControlBlock();

		task_func();

		ctrl->result_promise.set_value();

		// Reset the local copy of the control block to indicate finished
		ctrl.reset();
	}

	bool IsFinished() const
	{
		return !ctrl;
	}

	TaskResult<void> GetResult()
	{
		return { ctrl };
	}

private:
	std::shared_ptr< TaskResultControlBlock<void> > CreateControlBlock() const
	{
		return std::make_shared< TaskResultControlBlock<void> >();
	}
};

template<class Func, class... Args>
std::unique_ptr< TaskFunction<decltype( std::declval<Func>()( std::declval<Args>()... ) )> >
make_task_function(Func&& func, Args&&... args)
{
	typedef decltype( std::declval<Func>()( std::declval<Args>()... ) )
		result_type;

	std::unique_ptr<TaskFunction<result_type> > taskfunc{ new TaskFunction< result_type >(
			std::forward<Func>(func),
			std::forward<Args>(args)... ) };

	return taskfunc;
}

class Task
{
private:
	class GenericTaskContext
		: public TaskContextBase
	{
	public:
		GenericTaskContext()
			: TaskContextBase( nullptr )
		{}

		GenericTaskContext(std::unique_ptr<TaskFunctionBase> te)
			: TaskContextBase( std::move(te) )
		{}

		void Invoke()
		{
			if (taskfunc)
				taskfunc->Run();
		}

		void Yield()
		{
			// TODO: do nothing
		}
	};

private:
	std::shared_ptr<TaskContextBase> context;

public:
	struct GenericTag {};
	struct CoroutineTag {};

public:
	Task() = default;

	// Auto deduced Generic
	template<class Func, class... Args,
	         typename = typename std::enable_if<
		         !std::is_base_of< typename std::remove_reference<Func>::type, Task >::value &&
		         !std::is_same< typename std::remove_reference<Func>::type,
		                        std::shared_ptr<TaskFunctionBase>
		                         >::value
	                                           >::type >

	Task(Func&& func, Args&&... args)
		: context{ std::make_shared<GenericTaskContext>(
			         make_task_function(std::forward<Func>(func), std::forward<Args>(args)...) ) }
	{}

	template<class TFunc,
	         typename = typename std::enable_if<
		         std::is_base_of< TaskFunctionBase,
		                          typename std::remove_reference<TFunc>::type
		                        >::value
	                                           >::type
	        >
	Task(std::unique_ptr<TFunc> tf)
		: context( std::make_shared<GenericTaskContext>(std::move(tf)) )
	{}

	// Explicitly Generic
	template<class Func, class... Args>
	Task(GenericTag, Func&& func, Args&&... args)
		: context{ std::make_shared<GenericTaskContext>(
			make_task_function(std::forward<Func>(func), std::forward<Args>(args)...) ) }
	{}

	template<class TFunc>
	Task(GenericTag, std::unique_ptr<TFunc> tf)
		: context( std::make_shared<GenericTaskContext>( std::move(tf) ) )
	{}

#ifdef AS_USE_COROUTINE_TASKS
	// Explicity Coroutine
	template<class Func, class... Args>
	Task(CoroutineTag, Func&& func, Args&&... args)
		: context( std::make_shared<CoroutineTaskContext>(
			           make_task_function(std::forward<Func>(func), std::forward<Args>(args)...) ) )
	{}


	template<class TFunc,
	         typename = typename std::enable_if<
		         std::is_base_of< TaskFunctionBase,
		                          typename std::remove_reference<TFunc>::type
		                        >::value
	                                           >::type
	        >
	Task(CoroutineTag, std::unique_ptr<TFunc> tf)
		: context( std::make_shared<CoroutineTaskContext>( std::move(tf) ) )
	{}
#endif // AS_USE_COROUTINE_TASKS

	~Task() = default;

	Task(Task&&) = default;
	Task(Task const&) = default;

	Task& operator=(Task&&) = default;
	Task& operator=(Task const&) = default;

	void Invoke()
	{
		context->Invoke();
	}

	void Yield()
	{
		context->Yield();
	}

	bool IsFinished() const
	{
		return context->IsFinished();
	}
};

} // namespace as

#endif // AS_TASK_HPP
