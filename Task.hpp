#ifndef AS_TASK_HPP
#define AS_TASK_HPP

#include <memory>
#include <future>
#include <vector>
#include <functional>

// TODO: enable when platform boost supports context
//#undef AS_USE_COROUTINE_TASKS

#include "TaskContext.hpp"
#include "TaskExecutor.hpp"

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

	template<class... Args>
	void Set(Args&&... args)
	{
		result_promise.set_value( std::forward<Args>(args)... );
	}
};

template<>
struct TaskResultControlBlock<void>
{
	std::promise<void> result_promise;
	std::shared_future<void> result_future{ result_promise.get_future() };

	template<class... Args>
	void Set(Args&&... args)
	{
		result_promise.set_value();
	}
};

template<class T>
class TaskResult
{
	std::shared_ptr< TaskResultControlBlock<T> > ctrl;

	template<class U> friend struct TaskExecutor;

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

	template<class U> friend struct TaskExecutor;

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
struct TaskExecutor
	: public TaskExecutorBase
{
	std::shared_ptr< TaskResultControlBlock<Ret> > ctrl;
	std::function<Ret()> task_func;

	template<class Func, class... Args>
	TaskExecutor( Func&& func, Args&&... args )
		: ctrl( CreateControlBlock() )
		, task_func( std::bind(std::forward<Func>(func), std::forward<Args>(args)... ) )
	{}

	void Run()
	{
		if ( !ctrl )
			ctrl = CreateControlBlock();

		ctrl->result.reset( new Ret{ std::move( task_func() ) } );

		ctrl->result_promise.set_value( *ctrl->result );

		// Reset the local copy of the control block to indicate finished
		ctrl.reset();
	}

	bool IsFinished() const
	{
		return !ctrl;
	}

	std::shared_ptr< TaskResultControlBlock<Ret> > CreateControlBlock() const
	{
		return std::make_shared< TaskResultControlBlock<Ret> >();
	}

	TaskResult<Ret> GetResult()
	{
		return { ctrl };
	}
};

template<>
struct TaskExecutor<void>
	: public TaskExecutorBase
{
	std::shared_ptr< TaskResultControlBlock<void> > ctrl;
	std::function<void()> task_func;

	template<class Func, class... Args>
	TaskExecutor( Func&& func, Args&&... args )
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

	std::shared_ptr< TaskResultControlBlock<void> > CreateControlBlock() const
	{
		return std::make_shared< TaskResultControlBlock<void> >();
	}

	TaskResult<void> GetResult()
	{
		return { ctrl };
	}
};

template<class Func, class... Args>
std::shared_ptr< TaskExecutorBase >
make_executor(Func&& func, Args&&... args)
{
	return std::make_shared< TaskExecutor< decltype( std::declval<Func>()( std::declval<Args>()... ) ) > >(
		std::forward<Func>(func),
		std::forward<Args>(args)... );
}

class Task
{
private:
	class GenericTaskContext
		: public TaskContext
	{
		std::shared_ptr<TaskExecutorBase> taskexec;

	public:
		GenericTaskContext()
			: taskexec()
		{}

		GenericTaskContext(std::shared_ptr<TaskExecutorBase> te)
			: taskexec( te )
		{}

		void Invoke()
		{
			if (taskexec)
				taskexec->Run();
		}

		void Yield()
		{
			// TODO: do nothing
		}
	};

private:
	std::shared_ptr<TaskExecutorBase> executor;
	std::shared_ptr<TaskContext> context;

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
		                        std::shared_ptr<TaskExecutorBase>
		                         >::value
	                                           >::type >

	Task(Func&& func, Args&&... args)
		: executor{ make_executor(std::forward<Func>(func), std::forward<Args>(args)...) }
		, context{ std::make_shared<GenericTaskContext>(executor) }
	{}

	template<class TEConcept,
	         typename = typename std::enable_if<
		         std::is_base_of< TaskExecutorBase,
		                          typename std::remove_reference<TEConcept>::type
		                        >::value
	                                           >::type
	        >
	Task(std::shared_ptr<TEConcept> te)
		: executor( te )
		, context( std::make_shared<GenericTaskContext>(executor) )
	{}

	// Explicitly Generic
	template<class Func, class... Args>
	Task(GenericTag, Func&& func, Args&&... args)
		: executor{ std::make_shared< TaskExecutor< decltype( std::declval<Func>()( std::declval<Args>()... ) ) > >(
			                                      std::forward<Func>(func),
			                                      std::forward<Args>(args)... ) }
		, context{ std::make_shared<GenericTaskContext>(executor) }
	{}

	template<class TEConcept>
	Task(GenericTag, std::shared_ptr<TEConcept> te)
		: executor( te )
		, context( std::make_shared<GenericTaskContext>(executor) )
	{}

#ifdef AS_USE_COROUTINE_TASKS
	// Explicity Coroutine
	template<class Func, class... Args>
	Task(CoroutineTag, Func&& func, Args&&... args)
		: executor( std::make_shared< TaskExecutor< decltype( std::declval<Func>()( std::declval<Args>()... ) ) > >(
			            std::forward<Func>(func),
			            std::forward<Args>(args)... ) )
		, context( std::make_shared<CoroutineTaskContext>(executor) )
	{}


	template<class TEConcept,
	         typename = typename std::enable_if<
		         std::is_base_of< TaskExecutorBase,
		                          typename std::remove_reference<TEConcept>::type
		                        >::value
	                                           >::type
	        >
	Task(CoroutineTag, std::shared_ptr<TEConcept> te)
		: executor( te )
		, context( std::make_shared<CoroutineTaskContext>(executor) )
	{}
#endif // AS_USE_COROUTINE_TASKS

	~Task() = default;

	Task(Task&&) = default;
	Task(Task const&) = default;

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
		return executor->IsFinished();
	}
};

} // namespace as

#endif // AS_TASK_HPP
