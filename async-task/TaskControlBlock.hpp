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

class PostResult
{
public:
	template<class Ret>
	void operator()(std::function<Ret()>& func)
	{
		func();
	}
};

struct callable
{
	virtual ~callable() {}
	virtual void operator()() = 0;
};

template<class Func>
struct callable_base
	: public callable
{
	Func func;

	callable_base(Func&& f)
		: func( std::move(f) )
	{}

	virtual void operator()()
	{
		func();
	}
};

template<class Ret>
class BaseInvoker
	: public Invoker
{
	//std::function<Ret()> func;
	std::unique_ptr<callable> func;

public:
	template<class Func>
	BaseInvoker(Func&& f)
		: func( new callable_base<Func>( std::forward<Func>(f) ) )
	{}

	virtual TaskStatus operator()()
	{
		(*func)();
		return TaskStatus::Finished;
	}
};

} // namespace as

#endif // AS_TASK_CONTROL_BLOCK_HPP
