//
//  Task.hpp - Asynchronous Tasks and TaskResult<>'s
//
//  Copyright (c) 2015 Brian Fransioli
//
//  Distributed under the Boost Software License, Version 1.0.
//  See accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt
//

#ifndef AS_TASK_HPP
#define AS_TASK_HPP

#include <memory>
// TODO: enable when platform boost supports context
//#undef AS_USE_COROUTINE_TASKS

#include "TaskImpl.hpp"
#include "TaskControlBlock.hpp"
#include "TaskStatus.hpp"
#include "Channel.hpp"

#ifdef AS_USE_COROUTINE_TASKS
#include "CoroutineTaskImpl.hpp"
#endif // AS_USE_COROUTINE_TASKS

namespace as {

struct TaskStorage
{
	static constexpr int ss_size = 32;

	static constexpr int storage_size =
		( sizeof(char[ss_size]) < sizeof(void *) )
		? sizeof(void *)
		: sizeof(char[ss_size]);

	typedef std::aligned_storage<sizeof(char[storage_size]), alignof(char[storage_size])>::type storage_type;

	storage_type storage;

	const void *get() const
	{
		return std::addressof(storage);
	}

	void *get()
	{
		return std::addressof(storage);
	}

	template<class Functor>
	const Functor& get() const
	{
		return *static_cast<const Functor *>( get() );
	}

	template<class Functor>
	Functor& get()
	{
		return *static_cast<Functor *>( get() );
	}
};

class TaskBase
{
protected:

	enum operation_t {
		clone_t,
		move_t,
		invoke_t,
		destroy_t
	};

	template<class Functor>
	struct Manager
	{
		static const bool use_small_buffer = (
			sizeof( Functor ) <= sizeof(TaskStorage)
		                                     );

		typedef std::integral_constant<bool, use_small_buffer> use_small_buffer_t;

		static void
		manage(TaskStorage *dest, const TaskStorage *src, operation_t op)
		{
			switch(op) {
			case clone_t:
				clone( dest, src, use_small_buffer_t{} );
				break;

			case move_t:
				move( dest, src, use_small_buffer_t{} );
				break;

			case invoke_t:
				invoke( dest, use_small_buffer_t{} );
				break;

			case destroy_t:
				destroy( dest, use_small_buffer_t{} );
				break;

			default:
				assert( false );
				break;
			}
		}

		static void
		create(TaskStorage *storage, Functor&& f)
		{
			create( storage, std::move(f), use_small_buffer_t{} );
		}

		static void
		create(TaskStorage *storage, Functor&& f, std::true_type)
		{
			new (storage) Functor( std::move(f) );
		}

		static void
		create(TaskStorage *storage, Functor&& f, std::false_type)
		{
			storage->get<Functor *>() = new Functor( std::move(f) );
		}

		static void
		invoke(TaskStorage *storage, std::true_type)
		{
			storage->get<Functor>().Invoke();
		}

		static void
		invoke(TaskStorage *storage, std::false_type)
		{
			storage->get<Functor *>()->Invoke();
		}

		static void
		clone(TaskStorage *dest, const TaskStorage *src, std::true_type)
		{
			new ( dest->get() ) Functor( src->get<Functor>() );
		}

		static void
		clone(TaskStorage *dest, const TaskStorage *src, std::false_type)
		{
			dest->get<Functor *>() = new Functor( *src->get<Functor *>() );
		}

		static void
		move(TaskStorage *dest, const TaskStorage *src, std::true_type)
		{
			new ( dest->get() ) Functor( std::move( src->get<Functor>() ) );
		}

		static void
		move(TaskStorage *dest, const TaskStorage *src, std::false_type)
		{
			dest->get<Functor *>() = new Functor( std::move( *src->get<Functor *>() ) );
		}

		static void
		destroy(TaskStorage *storage, std::true_type)
		{
			storage->get<Functor>().~Functor();
		}

		static void
		destroy(TaskStorage *storage, std::false_type)
		{
			delete storage->get<Functor *>();
		}
	};

	typedef void (*TaskManager)(TaskStorage *dest, const TaskStorage *src, TaskBase::operation_t);

	TaskStorage storage;
	TaskManager manager;

	TaskBase()
		: storage()
		, manager(nullptr)
	{}

	template<class Impl>
	TaskBase(bool, Impl&& impl)
		: manager( Manager<Impl>::manage )
	{
		//static_assert( sizeof(Impl) <= sizeof(storage), "Functor size too large" );

		Manager<Impl>::create( &storage, std::forward<Impl>(impl) );
	}

	~TaskBase()
	{
		if ( manager )
			manager( &storage, nullptr, destroy_t );
	}

	TaskBase(TaskBase&& other)
	{
		if ( other.manager ) {
			other.manager( &storage, &other.storage, move_t );
			other.manager( &other.storage, nullptr, destroy_t );

			manager = other.manager;

			other.manager = nullptr;
		}
	}
};

class Task
	: public TaskBase
{
public:
	Task() = default;

	template<class Impl>
	Task(bool, Impl&& impl)
		: TaskBase(true, std::forward<Impl>(impl))
	{}

	Task(Task const&) = delete;
	Task(Task&&) = default;

	TaskStatus Invoke()
	{
		//assert( invoker );
		manager( &storage, nullptr, TaskBase::invoke_t );

		return TaskStatus::Finished;
	}

	void Yield()
	{
		// impl->Yield();
	}

	bool IsFinished() const
	{
		return true;
	}

	void Cancel()
	{
		// impl->Cancel();
	}
};

} // namespace as

#endif // AS_TASK_HPP
