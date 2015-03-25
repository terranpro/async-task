//
//  Channel.hpp - Multiple Result Channel/Queue
//
//  Copyright (c) 2015 Brian Fransioli
//
//  Distributed under the Boost Software License, Version 1.0.
//  See accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt
//

#ifndef AS_CHANNEL_HPP
#define AS_CHANNEL_HPP

#include <memory>
#include <future>
#include <mutex>
#include <condition_variable>

#include "TaskStatus.hpp"

namespace as {

template<class T>
struct Channel
{
	typedef std::unique_ptr<T> result_type;

	std::vector< result_type > results;
	std::mutex results_mut;
	std::condition_variable results_cond;
	std::atomic<bool> finished;
	std::atomic<bool> canceled;

	Channel()
		: results()
		, results_mut()
		, results_cond()
		, finished(false)
		, canceled(false)
	{}

	bool IsOpen() const
	{
		return !( finished || canceled );
	}

	template<class U>
	void Put(TaskFuncResult<U>&& u)
	{
		std::lock_guard<std::mutex> lock( results_mut );

		results.push_back( std::move(u.ret) );

		Ping();
	}

	result_type Get()
	{
		std::unique_lock<std::mutex> lock( results_mut );

		results_cond.wait( lock, [=]() { return WaitConditionLocked(); } );

		if ( !results.size() )
			return nullptr;

		result_type res = std::move( results[0] );
		results.erase( results.begin() );

		return res;
	}

	void Ping()
	{
		results_cond.notify_all();
	}

	void Cancel()
	{
		canceled = true;
		Ping();
	}

	void Close()
	{
		finished = true;
		Ping();
	}

	void Wait()
	{
		std::unique_lock<std::mutex> lock( results_mut );

		results_cond.wait( lock, [=]() { return WaitConditionLocked(); } );
	}

	template<class Rep, class Period>
	WaitStatus WaitFor( std::chrono::duration<Rep,Period> const& dur ) const
	{
		std::unique_lock<std::mutex> lock( results_mut );

		auto waitres = results_cond.wait_for( lock, dur, [=]() { return WaitConditionLocked(); } );

		return waitres == std::cv_status::timeout
			? WaitStatus::Timeout
			: WaitStatus::Ready;
	}

private:
	bool WaitConditionLocked() const
	{
		return results.size() || finished || canceled;
	}
};

template<>
struct Channel<void>
{
	typedef bool result_type;

	std::vector<char> results;
	std::mutex results_mut;
	std::condition_variable results_cond;
	std::atomic<bool> finished;
	std::atomic<bool> canceled;

	Channel()
		: results()
		, results_mut()
		, results_cond()
		, finished(false)
		, canceled(false)
	{}

	bool IsOpen() const
	{
		return !( finished || canceled );
	}

	void Put(TaskFuncResult<void>)
	{
		std::lock_guard<std::mutex> lock( results_mut );
		results.push_back( {} );

		Ping();
	}

	result_type Get()
	{
		std::unique_lock<std::mutex> lock( results_mut );
		results_cond.wait( lock, [=]() { return WaitConditionLocked(); } );

		if ( results.size() ) {
			results.pop_back();
			return true;
		}

		return false;
	}

	void Ping()
	{
		results_cond.notify_all();
	}

	void Cancel()
	{
		canceled = true;
		Ping();
	}

	void Close()
	{
		finished = true;
		Ping();
	}

	void Wait()
	{
		std::unique_lock<std::mutex> lock( results_mut );

		results_cond.wait( lock, [=]() { return WaitConditionLocked(); } );
	}

	template<class Rep, class Period>
	WaitStatus WaitFor( std::chrono::duration<Rep,Period> const& dur ) const
	{
		std::unique_lock<std::mutex> lock( results_mut );

		auto waitres = results_cond.wait_for( lock, dur, [=]() { return WaitConditionLocked(); } );

		return waitres == std::cv_status::timeout
			? WaitStatus::Timeout
			: WaitStatus::Ready;
	}

private:
	bool WaitConditionLocked() const
	{
		return results.size() || finished || canceled;
	}
};

} // namespace as

#endif // AS_CHANNEL_HPP
