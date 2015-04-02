//
//  TaskStatus.hpp - Result Status for Multi-run Tasks
//
//  Copyright (c) 2015 Brian Fransioli
//
//  Distributed under the Boost Software License, Version 1.0.
//  See accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt
//

#ifndef AS_TASK_STATUS_HPP
#define AS_TASK_STATUS_HPP

#include <memory>
#include <type_traits>
#include <future>

namespace as {

enum WaitStatus {
	Deferred = static_cast<int>( std::future_status::deferred ),
	Ready = static_cast<int>( std::future_status::ready ),
	Timeout = static_cast<int>( std::future_status::timeout )
};

enum class TaskStatus {
	Finished,
	Repeat,
	Continuing,
	Canceled
};

template<class T>
struct TaskResult;

template<>
struct TaskResult<void>;

template<>
struct TaskResult<void>
{
	TaskStatus status;

	TaskResult()
		: status()
	{}

	TaskResult(TaskStatus s)
		: status(s)
	{}

	template<class U>
	operator TaskResult<U>()
	{
		return TaskResult<U>(*this);
	}
};

template<class T>
struct TaskResult
{
	TaskStatus status;
	std::unique_ptr<T> ret;

	TaskResult()
		: status()
		, ret()
	{}

	TaskResult(TaskStatus s)
		: status(s)
		, ret()
	{}

	TaskResult(TaskStatus s, T val)
		: status(s)
		, ret( new T{ val } )
	{}

	explicit TaskResult(T val)
		: status()
		, ret( new T{ val } )
	{}

	TaskResult(TaskResult<void> const& other)
		: status(other.status)
		, ret()
	{}
};

static const TaskResult<void> repeat{ TaskStatus::Repeat };

static const TaskResult<void> cancel{ TaskStatus::Canceled };

template<class T>
inline TaskResult< typename std::remove_reference<T>::type >
finished(T&& res)
{
	return TaskResult< typename std::remove_reference<T>::type >( TaskStatus::Finished, std::forward<T>(res) );
}

inline TaskResult<void>
finished()
{
	return TaskResult<void>( TaskStatus::Finished );
}

template<class T>
inline TaskResult< typename std::remove_reference<T>::type >
continuing(T&& res)
{
	return TaskResult< typename std::remove_reference<T>::type >( TaskStatus::Continuing, std::forward<T>(res) );
}

inline TaskResult<void>
continuing()
{
	return TaskResult<void>( TaskStatus::Continuing );
}

} // namespace as

#endif // AS_TASK_STATUS_HPP
