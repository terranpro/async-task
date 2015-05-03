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
#include "CallableTraits.hpp"

#include <memory>
#include <functional>
#include <tuple>

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

// Move the indices stuff soon
template <std::size_t... Is>
struct indices {};

template <std::size_t N, std::size_t... Is>
struct build_indices
	: build_indices<N-1, N-1, Is...> {};

template <std::size_t... Is>
struct build_indices<0, Is...> : indices<Is...>
{
	using type = indices<Is...>;
};

template<std::size_t Offset, std::size_t N, std::size_t... Is>
struct build_indices_offset
	: build_indices_offset< Offset, N-1, N-1, Is... >
{};

template<std::size_t Offset, std::size_t... Is>
struct build_indices_offset<Offset, Offset, Is... >
{
	using type = indices< Is... >;
};

template <class Func>
struct convert_functor
{
	// helper to decay a reference function to pointer to function
	typedef typename std::conditional<
		std::is_same<Func, typename std::decay<Func>::type>::value,
		std::decay<Func>,
		convert_functor<typename std::decay<Func>::type>
	                                 >::type::type type;
};

template<class Func>
struct invocation
{
	typedef typename convert_functor<Func>::type func_type;
	typedef FunctionSignature<Func> signature_type;
	typedef typename FunctionSignature<Func>::return_type result_type;

	func_type func;

	template<class F>
	explicit invocation(F&& f)
		: func( std::forward<F>(f) )
	{}

	invocation(invocation const&) = default;
	invocation(invocation&&) = default;

	template<class... A>
	result_type invoke(A&&... args)
	{
		return func( std::forward<A>(args)... );
	}

	template<class ArgTuple, size_t... Ids>
	result_type invoke_impl(ArgTuple&& arg_tuple, indices<Ids...>)
	{
		return func( std::get<Ids>( std::forward<ArgTuple>(arg_tuple) )... );
	}
};

template<class Func, class... Args>
struct full_invocation
	: public invocation<Func>
{
	typedef std::tuple< typename std::decay<Args>::type...> arg_tuple_type;

	typedef decltype( std::declval<Func>()( std::declval< typename std::decay<Args>::type >()... ) )
	result_type;

private:
	arg_tuple_type arg_tuple;

public:
	template<class F, class... A>
	explicit full_invocation(F&& f, A&&... args)
		: invocation<Func>( std::forward<F>(f) )
		, arg_tuple( std::forward<A>(args)... )
	{}

	result_type invoke()
	{
		constexpr int TSize = std::tuple_size< arg_tuple_type >::value;

		return this->invoke_impl( arg_tuple, typename build_indices<TSize>::type{} );
	}
};

template<class... Invokers>
struct chain_invocation;

template<class First, class Second, class... Invokers>
struct chain_invocation<First, Second, Invokers...>
	: chain_invocation<Second, Invokers...>
{
	typedef chain_invocation<Second, Invokers...> base_type;

	typedef typename base_type::result_type result_type;

	First inv1;

	template<class F, class S, class... Is>
	chain_invocation(F&& i1, S&& i2, Is&&... invks)
		: base_type( std::forward<S>(i2), std::forward<Is>(invks)... )
		, inv1( std::forward<F>(i1) )
	{}

	template<class... Args>
	result_type invoke(Args&&... args)
	{
		return do_invoke( typename HasArg< typename Second::func_type >::type{},
		                  std::forward<Args>(args)... );
	}

private:
	template<class... Args>
	result_type do_invoke( std::true_type, Args&&... args )
	{
		return base_type::invoke( inv1.invoke( std::forward<Args>(args)... ) );
	}

	template<class... Args>
	result_type do_invoke( std::false_type, Args&&... args )
	{
		inv1.invoke( std::forward<Args>(args)... );
		return base_type::invoke();
	}
};

// TODO: this specialization is awkward and was difficult to get
// right; it needs to be initialized with a fully ready
// full_invocation or invocation
template<class First>
struct chain_invocation<First>
{
	typedef typename First::result_type result_type;

	First first;

	explicit chain_invocation(First&& f)
		: first(std::move(f))
	{}

	template<class... Args>
	result_type invoke(Args&&... args)
	{
		return first.invoke( std::forward<Args>(args)... );
	}

	template<class... Args>
	result_type operator()(Args&&... args)
	{
		return invoke( std::forward<Args>(args)... );
	}
};

template<class Callables, class Args>
struct invoker_builder;

template<class FirstCallable, class... Callables, class... Args>
struct invoker_builder< std::tuple<FirstCallable, Callables...>, std::tuple<Args...> >
{
	typedef full_invocation<FirstCallable, Args...> inv1_type;
	typedef typename inv1_type::result_type inv1_result_type;
	typedef chain_invocation< inv1_type, invocation<Callables>... >  chain_type;
	typedef chain_type result_type;

	template<class... Cs, class... A>
	chain_type build_chain(inv1_type&& c1, Cs&&... cs)
	{
		return chain_type( std::move(c1), invocation<Callables>{ std::forward<Cs>(cs) }... );
	}

	template<class F, class... A>
	result_type build(F&& c, Callables const&... cs, A&&... args)
	{
		static_assert( sizeof...(A) == sizeof...(Args), "Argument count mismatch" );

		return build_chain( inv1_type( std::forward<F>(c), std::forward<A>(args)... ), cs... );
	}
};

template<class Exec, class Func>
struct PostTask
{
	typedef Exec executor_type;
	typedef Func function_type;

	invocation<Func> func;
	Exec *executor;

	template<class F>
	PostTask(Exec *ex, F&& f)
		: func( std::forward<F>(f) )
		, executor(ex)
	{}

	TaskStatus Invoke()
	{
		func.invoke();

		return TaskStatus::Finished;
	}

	void Yield()
	{}

	void Cancel()
	{}
};

template<class Exec, class Func>
struct AsyncTask
{
	typedef Exec executor_type;
	typedef Func function_type;

	invocation<Func> func;
	Exec *executor;

	AsyncTask(Exec *ex, Func func)
		: func( std::move(func) )
		, executor(ex)
	{}

};

} // namespace as

#endif // AS_TASK_IMPL_HPP
