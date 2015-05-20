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

#ifndef AS_ASYNC_RESULT_HPP
#define AS_ASYNC_RESULT_HPP

#include <memory>
#include <mutex>
#include <condition_variable>

namespace as {

struct AsyncResultStorageBase
{
	bool is_set_;

	AsyncResultStorageBase()
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
struct AsyncResultStorage
	: AsyncResultStorageBase
{
	std::unique_ptr<Ret> res;

	template<class Func>
	void operator()(Func&& func)
	{
		res.reset( new Ret( func() ) );
	}

	template<class R>
	void set(R&& r)
	{
		res.reset( new Ret( std::forward<R>(r) ) );
		AsyncResultStorageBase::set();
	}

	Ret& get() const
	{
		return *res;
	}
};

template<>
struct AsyncResultStorage<void>
	: AsyncResultStorageBase
{
	template<class Func>
	void operator()(Func&& func)
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
	AsyncResultStorage<Ret> storage;

public:
	template<class R>
	void set(R&& r)
	{
		std::unique_lock<std::mutex> lock( mut );
		storage.set( std::forward<R>(r) );

		cond.notify_all();
	}

	Ret get()
	{
		std::unique_lock<std::mutex> lock( mut );
		cond.wait( lock, [=]() { return storage.is_set(); } );
		return storage.get();
	}
};

template<>
class AsyncResult<void>
{
	std::mutex mut;
	std::condition_variable cond;
	bool result_set;

public:
	void set()
	{
		std::unique_lock<std::mutex> lock( mut );
		result_set = true;

		cond.notify_all();
	}

	void get()
	{
		std::unique_lock<std::mutex> lock( mut );
		cond.wait( lock, [=]() { return result_set; } );
	}
};

} // namespace as

#endif // AS_ASYNC_RESULT_HPP
