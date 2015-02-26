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

#include "GlibExecutor.hpp"

#include <atomic>
#include <mutex>
#include <utility>

#include <cassert>

namespace as {

/// Create a Task and deduced TaskResult<> pair
template<class TaskTag, class Func, class... Args>
std::pair<
         Task,
         TaskResult< decltype( std::declval<Func>()(std::declval<Args>()...) ) >
        >
make_task_pair(TaskTag, Func&& func, Args&&... args)
{
	auto tf = make_task_function( std::forward<Func>(func),
	                              std::forward<Args>(args)... );
	auto result = tf->GetResult();

	return std::make_pair( Task{ TaskTag{}, std::move(tf) }, result );
}

/// Dispatch a callback in a thread context, i.e. an ExecutionContext
template<class Func, class... Args>
TaskResult< decltype( std::declval<Func>()(std::declval<Args>()...) ) >
async(Executor& context, Func&& func, Args&&... args)
{
	auto tr_pair = make_task_pair( Task::GenericTag{},
	                               std::forward<Func>(func),
	                               std::forward<Args>(args)... );

	context.Schedule( tr_pair.first );

	return tr_pair.second;
}

/// Dispatch a callback in thread; overload for shared_ptr context
template<class Func, class... Args>
TaskResult< decltype( std::declval<Func>()(std::declval<Args>()...) ) >
async(std::shared_ptr<Executor> context, Func&& func, Args&&... args)
{
	return async( *context, std::forward<Func>(func), std::forward<Args>(args)... );
}

/// Dispatch a callback in a new thread context (via GlibExecutionContext)
template<class Func, class... Args>
TaskResult< decltype( std::declval<Func>()(std::declval<Args>()...) ) >
async(Func&& func, Args&&... args)
{
	GlibExecutor c;

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
class AsyncPtr
{
	template<class U>
	friend class AsyncPtr;

	struct AsyncPtrImpl
	{
		std::mutex mut;
		std::unique_ptr<T> data;

		AsyncPtrImpl()
			: mut()
			, data()
		{}

	};

	template<class U>
	struct Setter
	{
		TaskResult<U> result;

		Setter(TaskResult<U> r)
			: result(r)
		{}

		U *operator()()
		{
			return new U( result.Get() );
		}
	};

	template<class U>
	struct Setter<U *>
	{
		TaskResult<U *> result;

		Setter(TaskResult<U *> r)
			: result(r)
		{}

		U *operator()()
		{
			return result.Get();
		}
	};

private:
	std::shared_ptr<AsyncPtrImpl> impl;
	mutable TaskResult<T> result;
	std::function<T *()> setter;

private:

	// template<class U>
	// U *Setter( TaskResult<U *> result )
	// {
	// 	return result.Get();
	// }

	// template<class U>
	// U *Setter( TaskResult<U> result )
	// {
	// 	return new U( result.Get() );
	// }

public:
	AsyncPtr( TaskResult<T> res )
		: impl( std::make_shared<AsyncPtrImpl>() )
		, result( std::move(res) )
		, setter( [=]() { return Setter<T>(result)(); } )
	{}

	AsyncPtr( std::unique_ptr<T> ptr )
		: impl( std::make_shared<AsyncPtrImpl>() )
		, result()
		, setter()
	{
		impl->data.reset( ptr.release() );
	}

	template<class U,
	         typename = typename std::enable_if<
		         std::is_convertible<U, T>::value
	                                           >::type
	        >
	AsyncPtr( TaskResult<U> res )
		: impl( std::make_shared<AsyncPtrImpl>() )
		, result()
		, setter( [=]() { return Setter<U>(res)(); } )
	{}

	template<class U,
	         typename = typename std::enable_if<
		         std::is_convertible<U, T>::value
	                                           >::type
	        >
	AsyncPtr( TaskResult<U *>&& res )
		: impl( std::make_shared<AsyncPtrImpl>() )
		, result()
		, setter( [=]() { return Setter<U *>( res )(); } )
	{}

	template<class U,
	         typename = typename std::enable_if<
		         std::is_convertible<U, T>::value
	                                           >::type
	        >
	AsyncPtr( AsyncPtr<U>&& other )
		: impl( std::make_shared<AsyncPtrImpl>() )
		, result()
		, setter( [=]()
		          {
			          // TODO: consider gutting the members and not copying
			          // AsyncPtr<U> into lambda
			          if ( other.result.Valid() )
				          return Setter<U>( other.result )();

			          return other.setter();
		          } )
	{}

	void Sync() const
	{
		std::lock_guard<std::mutex> lock{ impl->mut };

		if ( impl->data )
			return;

		assert( setter );

		impl->data.reset( setter() );
	}

	// This is questionable in need/utility
	explicit operator bool() const {
		return ( impl && impl->data ) || setter || result.Valid();
	}

	// These two overloads are questionable, they hold no lock and allow
	// read-only access; the first is also iffy for implicit conversions
	operator const T&() const {
		Sync();
		return *impl->data;
	}

	const T& operator*() const {
		Sync();
		return *impl->data;
	}

	AsyncProxyObject<T> operator->() {
		Sync();
		return CreateAsyncProxy( &(*impl->data), impl->mut );
	}

	AsyncProxyObject<T> const operator->() const {
		Sync();
		return CreateAsyncProxy( &(*impl->data), impl->mut );
	}

	AsyncProxyObject<T> GetProxy() const
	{
		Sync();
		return CreateAsyncProxy( &(*impl->data), impl->mut );
	}

	T& Direct() const {
		Sync();

		return *impl->data;
	}
};

// Assist with construction (args are copied, not forwarded, to be
// compatible with std::bind()'s decay copy)
template<class T, class... Args>
T *make_async_helper(Args... args)
{
	return new T( std::forward<Args>(args)... );
}

// Construction done using constructor directly
template<class T, class... Args,
         typename = typename std::enable_if<
	         std::is_constructible<T, Args...>::value
                                  >::type
        >
AsyncPtr<T> make_async(Executor& ctxt, Args&&... args)
{
	auto bndfunc = std::bind( make_async_helper<T, Args...>,
	                          std::forward<Args>(args)... );
	auto res = as::async( ctxt, std::move(bndfunc) );

	return { res };
}

template<class T, class... Args,
         typename = typename std::enable_if<
	         std::is_constructible<T, Args...>::value
                                           >::type
        >
AsyncPtr<T> make_async(Args&&... args)
{
	auto bndfunc = std::bind( make_async_helper<T, Args...>,
	                          std::forward<Args>(args)... );
	auto res = as::async( std::move(bndfunc) );

	return std::move( res );
}

// Construction done via callable expression return result
template<class T, class Func, class... Args,
         typename = typename std::enable_if<
	         ! std::is_constructible<T, Func, Args...>::value
                                  >::type
        >
AsyncPtr<T> make_async(Executor& ctxt, Func&& func, Args&&... args)
{
	return { as::async( ctxt,
	                    std::forward<Func>(func),
	                    std::forward<Args>(args)... ) };
}

template<class T, class Func, class... Args,
         typename = typename std::enable_if<
	         ! std::is_constructible<T, Func, Args...>::value
                                  >::type
        >
AsyncPtr<T> make_async(Func&& func, Args&&... args)
{
	return { as::async( std::forward<Func>(func),
	                    std::forward<Args>(args)... ) };
}

} // namespace as

#endif // AS_ASYNC_HPP
