#ifndef AS_ASYNC_HPP
#define AS_ASYNC_HPP

#include "GlibExecutionContext.hpp"

#include <atomic>
#include <mutex>
#include <utility>

namespace as {

/// Create a Task and deduced TaskResult<> pair
template<class TaskTag, class Func, class... Args>
std::pair<
         Task,
         TaskResult< decltype( std::declval<Func>()(std::declval<Args>()...) ) >
        >
make_task_pair(TaskTag, Func&& func, Args&&... args)
{
	auto te =
		std::make_shared< TaskExecutor< decltype( std::declval<Func>()(std::declval<Args>()...) ) > >(
			std::forward<Func>(func),
			std::forward<Args>(args)... );

	return std::make_pair( Task{ TaskTag{}, te }, te->GetResult() );
}

/// Dispatch a callback in a thread context, i.e. an ExecutionContext
template<class Func, class... Args>
TaskResult< decltype( std::declval<Func>()(std::declval<Args>()...) ) >
async(ExecutionContext& context, Func&& func, Args&&... args)
{
	auto tr_pair = make_task_pair( Task::GenericTag{},
	                               std::forward<Func>(func),
	                               std::forward<Args>(args)... );

	context.AddTask( tr_pair.first );

	return tr_pair.second;
}

/// Dispatch a callback in thread; overload for shared_ptr context
template<class Func, class... Args>
TaskResult< decltype( std::declval<Func>()(std::declval<Args>()...) ) >
async(std::shared_ptr<ExecutionContext> context, Func&& func, Args&&... args)
{
	return async( *context, std::forward<Func>(func), std::forward<Args>(args)... );
}

/// Dispatch a callback in a new thread context (via GlibExecutionContext)
template<class Func, class... Args>
TaskResult< decltype( std::declval<Func>()(std::declval<Args>()...) ) >
async(Func&& func, Args&&... args)
{
	GlibExecutionContext c;

	return async( c,
	              std::forward<Func>(func),
	              std::forward<Args>(args)... );
}

template<class Obj>
class AsyncProxyObject
{
private:
	Obj *obj;
	mutable std::unique_lock<std::mutex> unlock;

public:
	AsyncProxyObject(Obj *obj, std::unique_lock<std::mutex> lock)
		: obj(obj), unlock(std::move(lock))
	{}

	AsyncProxyObject(AsyncProxyObject const& other)
		: obj(other.obj)
		, unlock(std::move(other.unlock))
	{}

	Obj* operator->() const
	{
		return obj;
	}

	Obj& operator*() const
	{
		return *obj;
	}
};

template<class Obj, class Mutex>
AsyncProxyObject< Obj >
CreateAsyncProxy( Obj *obj, Mutex& mut )
{
	std::unique_lock<std::mutex> lock{ mut };

	return AsyncProxyObject< Obj >{ obj, std::move(lock) };
}

template<class T>
struct AsyncHandle
{
private:
	std::shared_ptr< std::shared_ptr<T> > data;
	std::shared_ptr<std::mutex> mut;
	mutable TaskResult<T> result;

public:
	AsyncHandle( TaskResult<T> res )
		: data( std::make_shared< std::shared_ptr<T> >() )
		, mut( std::make_shared< std::mutex >() )
		, result( std::move(res) )
	{}

	void Sync() const
	{
		std::lock_guard<std::mutex> lock{*mut};

		if ( *data )
			return;

		*data = std::make_shared<T>( result.Get() );
	}

	// This is questionable in need/utility
	explicit operator bool() const {
		return result.Valid();
	}

	// These two overloads are questionable, they hold no lock and allow
	// read-only access; the first is also iffy for implicit conversions
	operator const T&() const {
		Sync();
		return **data;
	}

	const T& operator*() const {
		Sync();
		return **data;
	}

	AsyncProxyObject<T> operator->() {
		Sync();
		return CreateAsyncProxy( &(**data), *mut );
	}

	AsyncProxyObject<T> const operator->() const {
		Sync();
		return CreateAsyncProxy( &(**data), *mut );
	}

	AsyncProxyObject<T> GetProxy() const
	{
		Sync();
		return CreateAsyncProxy( &(**data), *mut );
	}

	T& Direct() const {
		Sync();

		return **data;
	}
};

} // namespace as

#endif // AS_ASYNC_HPP
