//
//  TaskControlBlock.hpp - Task Control Block that holds functor,
//  synchronization, and status
//
//  Copyright (c) 2015 Brian Fransioli
//
//  Distributed under the Boost Software License, Version 1.0.
//  See accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt
//

#ifndef AS_TASK_CONTROL_BLOCK_HPP
#define AS_TASK_CONTROL_BLOCK_HPP

#include "TaskStatus.hpp"
#include "Channel.hpp"

#include <type_traits>

#include <cassert>

namespace as {

class Invoker
{
public:
	virtual TaskStatus operator()() = 0;

protected:
	virtual ~Invoker() {}
};

struct BaseInvokerStorage
{
	bool is_set_;

	BaseInvokerStorage()
		: is_set_(false)
	{}

	void set()
	{
		is_set_ = true;
	}

	bool is_set() const
	{
		return is_set_;
	}
};

template<class Ret>
struct InvokerStorage
	: BaseInvokerStorage
{
	std::unique_ptr<Ret> res;

	void operator()(std::function<Ret()>& func)
	{
		res.reset( new Ret( func() ) );
	}

	Ret& get() const
	{
		return *res;
	}
};

template<>
struct InvokerStorage<void>
	: BaseInvokerStorage
{
	void operator()(std::function<void()>& func)
	{
		func();

		this->set();
	}

	void get() const {}
};

template<class Ret>
class AsyncResult
{
	std::mutex mut;
	std::condition_variable cond;
	InvokerStorage<Ret> storage;

public:
	void operator()(std::function<Ret()>& func)
	{
		storage( func );

		storage.set();

		cond.notify_all();
	}

	Ret get()
	{
		std::unique_lock<std::mutex> lock( mut );
		cond.wait( lock, [=]() { return storage.is_set(); } );
		return storage.get();
	}
};

struct callable
{
	virtual ~callable() {}
	virtual void operator()() = 0;
};

template<class Func>
struct callable_impl
	: public callable
{
	Func func;

	callable_impl(Func&& f)
		: func( std::move(f) )
	{}

	virtual void operator()()
	{
		func();
	}
};

template<class Func, class... Args>
std::unique_ptr<callable>
make_callable( Func&& func, Args&&... args )
{
	// auto binder = std::bind( std::forward<Func>(func),
	//                          std::forward<Args>(args)... );

	return std::unique_ptr<callable>( new callable_impl<Func>( std::forward<Func>(func) ) );
}

template<class T, class Alloc>
struct callable_deleter
{
	void operator()(T *obj) const
	{
		obj->~T();
		typename Alloc::template rebind<T>::other rebind_alloc;
		rebind_alloc.deallocate( obj, 1 );
	}
};

template<class Alloc>
using callable_ptr = std::unique_ptr<callable, callable_deleter<callable, Alloc> >;

template<class Alloc, class Func, class... Args>
std::unique_ptr<callable, callable_deleter<callable, Alloc> >
make_callable( Func&& func, Args&&... args )
{
	auto binder = std::bind( std::forward<Func>(func),
	                         std::forward<Args>(args)... );

	typename Alloc::template rebind< decltype(binder) >::other rebind_alloc;

	auto ptr = rebind_alloc.allocate(1);

	auto obj = new(ptr) callable_impl<decltype(binder)>( std::move(binder) );

	return callable_ptr< Alloc >( obj );
}

template<class Ret>
class BaseInvoker
	: public Invoker
{
	std::unique_ptr<callable> func;

public:
	template<class Func>
	BaseInvoker(Func&& f)
		: func( new callable_impl<Func>( std::move(f) ) )
	{}

	BaseInvoker(BaseInvoker&&) = default;

	virtual ~BaseInvoker()
	{}

	virtual TaskStatus operator()()
	{
		(*func)();
		return TaskStatus::Finished;
	}
};

} // namespace as

#endif // AS_TASK_CONTROL_BLOCK_HPP
