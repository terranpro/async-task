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

namespace as {
inline namespace v1 {

enum class TaskStatus {
	Finished,
	Repeat,
	Continuing,
	Canceled
};

template<class T>
struct TaskFuncResult;

template<>
struct TaskFuncResult<void>;

template<>
struct TaskFuncResult<void>
{
	TaskStatus status;

	TaskFuncResult()
		: status()
	{}

	TaskFuncResult(TaskStatus s)
		: status(s)
	{}

	template<class U>
	operator TaskFuncResult<U>()
	{
		return TaskFuncResult<U>(*this);
	}
};

template<class T>
struct TaskFuncResult
{
	TaskStatus status;
	std::unique_ptr<T> ret;

	TaskFuncResult()
		: status()
		, ret()
	{}

	TaskFuncResult(TaskStatus s)
		: status(s)
		, ret()
	{}

	TaskFuncResult(TaskStatus s, T val)
		: status(s)
		, ret( new T{ val } )
	{}

	explicit TaskFuncResult(T val)
		: status()
		, ret( new T{ val } )
	{}

	TaskFuncResult(TaskFuncResult<void> const& other)
		: status(other.status)
		, ret()
	{}
};

static const TaskFuncResult<void> repeat{ TaskStatus::Repeat };
static const TaskFuncResult<void> cancel{ TaskStatus::Canceled };

template<class T>
inline TaskFuncResult< typename std::remove_reference<T>::type >
finished(T&& res)
{
	return TaskFuncResult< typename std::remove_reference<T>::type >( TaskStatus::Finished, std::forward<T>(res) );
}

inline TaskFuncResult<void>
finished()
{
	return TaskFuncResult<void>( TaskStatus::Finished );
}

template<class T>
inline TaskFuncResult< typename std::remove_reference<T>::type >
continuing(T&& res)
{
	return TaskFuncResult< typename std::remove_reference<T>::type >( TaskStatus::Continuing, std::forward<T>(res) );
}

inline TaskFuncResult<void>
continuing()
{
	return TaskFuncResult<void>( TaskStatus::Continuing );
}

} // inline namespace as::v1
} // namespace as

#endif // AS_TASK_STATUS_HPP
