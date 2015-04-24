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

	template<class Func>
	void operator()(Func&& func)
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
	template<class Func>
	void operator()(Func&& func)
	{
		func();

		this->set();
	}

	void get() const {}
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
	auto binder = std::bind( std::forward<Func>(func),
	                         std::forward<Args>(args)... );

	return std::unique_ptr<callable>( new callable_impl<decltype(binder)>( std::move(binder) ) );
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

template<class Ret>
class AsyncResult
{
	std::mutex mut;
	std::condition_variable cond;
	InvokerStorage<Ret> storage;

public:
	template<class Func>
	void operator()(Func&& func)
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

template<class Inv>
struct ResultSelector;



} // namespace as

#endif // AS_TASK_CONTROL_BLOCK_HPP
