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
#include <vector>

#include "TaskStatus.hpp"

namespace as {

template<class T>
struct ChannelImpl
{
	typedef std::unique_ptr<T> result_type;

	std::vector< result_type > results;
	std::mutex results_mut;
	std::condition_variable results_cond;
	std::atomic<bool> finished;
	std::atomic<bool> canceled;

	ChannelImpl()
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
	void Put(TaskResult<U>&& u)
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

	size_t Count() const
	{
		std::unique_lock<std::mutex> lock( results_mut );

		return results.size();
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
struct ChannelImpl<void>
{
	typedef bool result_type;

	std::vector<char> results;
	std::mutex results_mut;
	std::condition_variable results_cond;
	std::atomic<bool> finished;
	std::atomic<bool> canceled;

	ChannelImpl()
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

	void Put(TaskResult<void>)
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

template<class T>
class Channel
{
	std::shared_ptr< ChannelImpl<T> > impl;

public:
	typedef typename ChannelImpl<T>::result_type result_type;

public:
	Channel()
		: impl( std::make_shared< ChannelImpl<T> >() )
	{}

	bool IsOpen() const
	{
		return impl->IsOpen();
	}

	void Put(TaskResult<T> tfr)
	{
		impl->Put( std::move(tfr) );
	}

	result_type
	Get()
	{
		return impl->Get();
	}

	void Ping()
	{
		impl->Ping();
	}

	void Cancel()
	{
		impl->Cancel();
	}

	void Close()
	{
		impl->Close();
	}

	void Wait()
	{
		impl->Wait();
	}

	template<class Rep, class Period>
	WaitStatus WaitFor( std::chrono::duration<Rep,Period> const& dur ) const
	{
		return impl->WaitFor( dur );
	}
};

template<class T>
struct ChannelIterator
{
	typedef typename ChannelImpl<T>::result_type result_type;

	std::weak_ptr< ChannelImpl<T> > weak_channel;

	ChannelIterator()
		: weak_channel()
	{}

	ChannelIterator(std::shared_ptr< ChannelImpl<T> > channelimpl )
		: weak_channel(channelimpl)
	{}

	result_type operator*()
	{
		auto ch = weak_channel.lock();
		if ( ch )
			return ch->Get();
		// TODO:
	}

	void operator++()
	{}

	void operator++(int)
	{}

	bool operator!=(ChannelIterator const& other) const
	{
		return weak_channel != other.weak_channel;
	}

	explicit operator bool() const
	{
		return weak_channel;
	}
};

} // namespace as

#endif // AS_CHANNEL_HPP
