//
//  TaskImpl.hpp - Basic Task Context/Implementation
//
//  Copyright (c) 2015 Brian Fransioli
//
//  Distributed under the Boost Software License, Version 1.0.
//  See accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt
//

#ifndef AS_TASK_IMPL_HPP
#define AS_TASK_IMPL_HPP

#include "TaskControlBlock.hpp"

#include <memory>
#include <functional>
#include <tuple>

#include <boost/pool/pool_alloc.hpp>

namespace as {

class TaskImpl
{
public:
	virtual ~TaskImpl() {}

	virtual TaskStatus Invoke() = 0;
	virtual void Yield() = 0;
	virtual void Cancel() = 0;
	//virtual bool IsFinished() const = 0;
};

// template<class Invoker, class Result>

template< class Handler >
class TaskImplBase
	: public TaskImpl
{
protected:
	Handler handler;

public:
	TaskImplBase() = default;

	template<class Func, class... Args,
	         class =
	         typename std::enable_if< !std::is_same<TaskImplBase,
	                                                typename std::decay<Func>::type
	                                               >::value
	                                >::type
	        >
	TaskImplBase( Func&& func, Args&&... args )
		: handler( std::bind( std::forward<Func>(func),
		                      std::forward<Args>(args)... ) )
	{}

	TaskImplBase(TaskImplBase&&) = default;

	virtual ~TaskImplBase()
	{}

	virtual TaskStatus Invoke()
	{
		return handler();
	}

	virtual void Yield()
	{}

	virtual void Cancel()
	{
	}

};

// template<class Exec = void, class... >
// struct PostInvoker

template <typename Ret, typename F, typename Tuple, bool Done, int Total, int... N>
struct invoke_impl
{
	static Ret call(F f, Tuple && t)
	{
		return invoke_impl<Ret, F, Tuple, Total == 1 + sizeof...(N), Total, N..., sizeof...(N)>::call(f, std::forward<Tuple>(t));
	}
};

template <typename Ret, typename F, typename Tuple, int Total, int... N>
struct invoke_impl<Ret, F, Tuple, true, Total, N...>
{
	static Ret call(F f, Tuple && t)
	{
		return f(std::get<N>(std::forward<Tuple>(t))...);
	}
};


template<class Signature>
struct func_sig;

template<class Ret, class... Args>
struct func_sig<Ret(Args...)>
{
	typedef std::tuple<Args...> arg_tuple_type;
	typedef Ret return_type;
};

template<class Func, class...Args>
struct invocation_result_helper
{
	typedef decltype( std::declval<Func>()( std::declval<Args>()... ) ) result_type;
};

template<class Func, class... Args>
struct invocation_result_helper<Func, std::tuple<Args...>>
{
	typedef decltype( std::declval<Func>()( std::declval<Args>()... ) ) result_type;
};

template<class Func, class... Args>
struct invocation
{
	typedef std::tuple<Args...> arg_tuple_type;

	typedef decltype( std::declval<Func>()( std::declval<Args>()... ) ) result_type;

	Func func;
	arg_tuple_type arg_tuple;

	template<class... A>
	invocation(Func func, A&&... args)
		: func( std::move(func) )
		, arg_tuple( std::make_tuple<Args...>( std::forward<A>(args)... ) )
	{}

	result_type invoke()
	{
		constexpr int TSize = std::tuple_size< arg_tuple_type >::value;

		return invoke_impl<result_type, Func, arg_tuple_type, 0 == TSize, TSize>::call(func, std::move(arg_tuple) );
	}

	template<class... A>
	result_type invoke(A&&... args)
	{
		constexpr int TSize = std::tuple_size< arg_tuple_type >::value;

		return invoke_impl<result_type, Func, arg_tuple_type, 0 == TSize, TSize>::call(func, std::make_tuple( std::forward<A>(args)... ) );
	}
};

template<class FirstInvocation,
         class SecondInvocation,
         bool SecondHasParam = true>
struct chain_invoke
{
	typedef typename SecondInvocation::result_type result_type;

	FirstInvocation inv1;
	SecondInvocation inv2;

	chain_invoke(FirstInvocation i1,
	             SecondInvocation i2)
		: inv1( i1 )
		, inv2( i2 )
	{}

	result_type invoke()
	{
		inv2.invoke( inv1.invoke() );
	}
};

template<class Exec, class Func>
struct PostInvoker
	: public TaskImpl
{
	typedef Exec executor_type;

	invocation<Func> func;
	Exec *executor;
	std::shared_ptr<TaskImpl> next;

	// template<class Func>
	// PostInvoker(Func&& func)
	// 	: func( make_callable( std::forward<Func>(func) ) )
	// 	, executor(nullptr)
	// {}

	// template<class Func>
	// PostInvoker(Exec *ex, Func&& func, std::shared_ptr<TaskImpl> next = nullptr)
	// 	: func( make_callable( std::forward<Func>(func) ) )
	// 	, executor(ex)
	// 	, next(next)
	// {}

	PostInvoker(Exec *ex, Func func, std::shared_ptr<TaskImpl> next = nullptr)
		: func( std::move(func) )
		, executor(ex)
		, next(next)
	{}

	virtual TaskStatus Invoke()
	{
		func.invoke();

		return TaskStatus::Finished;
	}

	virtual void Yield()
	{}
	virtual void Cancel()
	{}
};

} // namespace as

#endif // AS_TASK_IMPL_HPP
