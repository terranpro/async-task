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

// #include "GlibExecutor.hpp"
#include "ThreadExecutor.hpp"

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
	// TODO: examine why launching 48+ GlibExecutor's might deadlock on
	// Cygwin; replaced default with ThreadExecutor's to start

	//GlibExecutor c;
	ThreadExecutor c;

	return async( c,
	              std::forward<Func>(func),
	              std::forward<Args>(args)... );
}

template<class Obj, class Mutex = std::recursive_mutex>
class AsyncProxyObject
{
private:
	Obj *obj;
	mutable std::unique_lock<Mutex> unlock;

public:
	AsyncProxyObject(Obj *obj, std::unique_lock<Mutex> lock)
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
	std::unique_lock<Mutex> lock{ mut };

	return AsyncProxyObject< Obj >{ obj, std::move(lock) };
}

struct AsyncPtrControlBlock
{
	typedef std::recursive_mutex MutexType;
	typedef std::unique_lock<MutexType> LockType;

	// Recursive mutex used to protect against deadlock if a
	// callback/signal tries to recursively access AsyncPtr<>
	std::recursive_mutex mut;
	void *data;
	std::once_flag flag;
	bool synced;

	AsyncPtrControlBlock()
		: mut()
		, data()
		, flag()
		, synced(false)
	{}

	virtual ~AsyncPtrControlBlock()
	{}

	virtual void SetResult() = 0;

	LockType Lock()
	{
		LockType lock{ mut };

		return lock;
	}

	void *Sync()
	{
		std::call_once( flag, [=]() {
				auto lock = Lock();
				SetResult();
				synced = true;
			} );

		return data;
	}
};

template<class T>
struct AsyncPtrSynchronizer
	: public AsyncPtrControlBlock
{
	TaskResult<T> result;
	std::unique_ptr<T> ptr;

	AsyncPtrSynchronizer(TaskResult<T> r)
		: result( std::move(r) )
	{}

	void SetResult()
	{
		ptr.reset( new T( result.Get() ) );
		data = ptr.get();

		// data = new T( result.Get() );
		// ptr.reset( static_cast<T *>(data) );
	}
};

template<class T>
struct AsyncPtrSynchronizer<T *>
	: public AsyncPtrControlBlock
{
	TaskResult<T *> result;
	std::unique_ptr<T> ptr;

	AsyncPtrSynchronizer(std::unique_ptr<T> ptr)
		: result()
	{
		data = ptr.release();
	}

	AsyncPtrSynchronizer(TaskResult<T *>&& r)
		: result( std::move(r) )
	{}

	void SetResult()
	{
		if (data)
			return;

		ptr.reset( result.Get() );
		data = ptr.get();

		// data = result.Get();
		// ptr.reset( static_cast<T *>(data) );
	}
};

template<class T>
struct AsyncPtrSynchronizer< std::unique_ptr<T> >
	: public AsyncPtrControlBlock
{
	TaskResult< std::unique_ptr<T> > result;
	std::unique_ptr<T> ptr;

	AsyncPtrSynchronizer(TaskResult< std::unique_ptr<T> >&& r)
		: result( std::move(r) )
	{}

	void SetResult()
	{
		if (data)
			return;

		ptr = std::move( result.Get() );
		data = ptr.get();
	}
};

template<class T>
class AsyncPtr
{
	template<class U>
	friend class AsyncPtr;

private:
	mutable T* ptr;
	std::shared_ptr<AsyncPtrControlBlock> impl;

public:
	AsyncPtr()
		: ptr()
		, impl()
	{}

	AsyncPtr( TaskResult<T> res )
		: ptr()
		, impl( std::make_shared< AsyncPtrSynchronizer<T> >(res) )
	{}

	AsyncPtr( std::unique_ptr<T> uptr )
		: ptr( uptr.get() )
		, impl( std::make_shared< AsyncPtrSynchronizer<T *> >(std::move(uptr)) )
	{}

	template<class U,
	         typename = typename std::enable_if<
		          ( std::is_base_of<T, U>::value ||
		            std::is_convertible<U, T>::value ) &&
		         !std::is_pointer<U>::value
	                                           >::type
	        >
	AsyncPtr( TaskResult< std::unique_ptr<U> > res )
		: ptr(), impl( std::make_shared<AsyncPtrSynchronizer< std::unique_ptr<U> > >( std::move(res) ) )
	{}

	template<class U,
	         typename = typename std::enable_if<
		         ( std::is_base_of<T, U>::value
		           || std::is_convertible<U, T>::value )
		&& !std::is_pointer<U>::value
	                                           >::type
	        >
	AsyncPtr( TaskResult<U> res )
		: ptr(), impl( std::make_shared<AsyncPtrSynchronizer<U> >(res) )
	{}

	template<class U,
	         typename = typename std::enable_if<
		         std::is_base_of<T, U>::value ||
		         std::is_convertible<U, T>::value
	                                           >::type
	        >
	AsyncPtr( TaskResult<U *>&& res )
		: ptr()
		, impl( std::make_shared<AsyncPtrSynchronizer<U *> >(std::move(res)) )
	{}

	template<class U,
	         typename = typename std::enable_if<
		         std::is_base_of<T, U>::value ||
		         std::is_convertible<U, T>::value
	                                           >::type
	        >
	AsyncPtr( AsyncPtr<U>&& other )
		: ptr( std::move(other.ptr) )
		, impl( std::move(other.impl) )
	{}

	template<class U,
	         typename = typename std::enable_if<
		         std::is_base_of<T, U>::value ||
		         std::is_convertible<U, T>::value
	                                           >::type
	        >
	AsyncPtr( AsyncPtr<U> const& other )
		: ptr( other.ptr )
		, impl( other.impl )
	{}

	void Sync() const
	{
		assert( impl );

		if (ptr)
			return;

		ptr = static_cast<T *>( impl->Sync() );
	}

	// This is questionable in need/utility
	explicit operator bool() const {
		return !!impl;
	}

	// These two overloads are questionable, they hold no lock and allow
	// read-only access; the first is also iffy for implicit conversions
	operator const T&() const {
		Sync();
		return *ptr;
	}

	const T& operator*() const {
		Sync();
		return *ptr;
	}

	AsyncProxyObject<T> operator->() {
		Sync();
		return AsyncProxyObject< T >( ptr, impl->Lock() );
	}

	AsyncProxyObject<T> const operator->() const {
		Sync();
		return AsyncProxyObject< T >( ptr, impl->Lock() );
	}

	AsyncProxyObject<T> GetProxy() const
	{
		Sync();
		return AsyncProxyObject< T >( ptr, impl->Lock() );
	}

	T& Direct() const {
		Sync();

		return *ptr;
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
