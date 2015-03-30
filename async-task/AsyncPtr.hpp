//
//  AsyncPtr.hpp - Async initialization of a shared object with auto locking
//
//  Copyright (c) 2015 Brian Fransioli
//
//  Distributed under the Boost Software License, Version 1.0.
//  See accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt
//

#ifndef AS_ASYNC_PTR_HPP
#define AS_ASYNC_PTR_HPP

namespace as {

template<class Obj, class Mutex = std::recursive_mutex>
class AsyncProxyObject
{
private:
	Obj *obj;
	std::unique_lock<Mutex> unlock;

public:
	AsyncProxyObject(Obj *obj, std::unique_lock<Mutex> lock)
		: obj(obj), unlock(std::move(lock))
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
	TaskFuture<T> result;
	std::unique_ptr<T> ptr;

	AsyncPtrSynchronizer(TaskFuture<T> r)
		: result( std::move(r) )
	{}

	void SetResult()
	{
		ptr.reset( new T( result.Get() ) );
		data = ptr.get();
	}
};

template<class T>
struct AsyncPtrSynchronizer<T *>
	: public AsyncPtrControlBlock
{
	TaskFuture<T *> result;
	std::unique_ptr<T> ptr;

	AsyncPtrSynchronizer(std::unique_ptr<T> ptr)
		: result()
	{
		data = ptr.release();
	}

	AsyncPtrSynchronizer(TaskFuture<T *>&& r)
		: result( std::move(r) )
	{}

	void SetResult()
	{
		if (data)
			return;

		ptr.reset( result.Get() );
		data = ptr.get();
	}
};

template<class T>
struct AsyncPtrSynchronizer< std::unique_ptr<T> >
	: public AsyncPtrControlBlock
{
	TaskFuture< std::unique_ptr<T> > result;
	std::unique_ptr<T> ptr;

	AsyncPtrSynchronizer(TaskFuture< std::unique_ptr<T> >&& r)
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
	AsyncPtr( TaskFuture< std::unique_ptr<U> > res )
		: ptr(), impl( std::make_shared<AsyncPtrSynchronizer< std::unique_ptr<U> > >( std::move(res) ) )
	{}

	template<class U,
	         typename = typename std::enable_if<
		         ( std::is_base_of<T, U>::value
		           || std::is_convertible<U, T>::value )
		&& !std::is_pointer<U>::value
	                                           >::type
	        >
	AsyncPtr( TaskFuture<U> res )
		: ptr(), impl( std::make_shared<AsyncPtrSynchronizer<U> >(res) )
	{}

	template<class U,
	         typename = typename std::enable_if<
		         std::is_base_of<T, U>::value ||
		         std::is_convertible<U, T>::value
	                                           >::type
	        >
	AsyncPtr( TaskFuture<U *>&& res )
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

#endif // AS_ASYNC_PTR_HPP
