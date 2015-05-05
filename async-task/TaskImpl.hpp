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

template<class Func, class... A>
auto invoke(Func&& f, A&&... args)
	-> decltype( std::forward<Func>(f)( std::forward<A>(args)... ) )
{
	return std::forward<Func>(f)( std::forward<A>(args)... );
}

template<class Func, class ArgTuple, size_t... Ids>
auto tuple_invoke_impl(Func&& f, ArgTuple&& arg_tuple, indices<Ids...>)
	-> decltype( std::forward<Func>(f)( std::get<Ids>( std::forward<ArgTuple>(arg_tuple) )... ) )
{
	return std::forward<Func>(f)( std::get<Ids>( std::forward<ArgTuple>(arg_tuple) )... );
}

template<class Func, class ArgTuple>
auto tuple_invoke(Func&& f, ArgTuple&& arg_tuple)
	-> decltype( tuple_invoke_impl( std::forward<Func>(f),
	                                std::forward<ArgTuple>(arg_tuple),
	                                typename build_indices< std::tuple_size<
	                                typename std::remove_reference<ArgTuple>::type
	                                >::value >::type{} ) )
{
	return tuple_invoke_impl( std::forward<Func>(f),
	                          std::forward<ArgTuple>(arg_tuple),
	                          typename build_indices< std::tuple_size<
	                          typename std::remove_reference<ArgTuple>::type
	                          >::value >::type{} );
}



template<class Func, class Next, class... Args>
auto chain_invoke_impl( Func&& f, Next&& n, std::true_type, Args&&... args )
	-> decltype( std::forward<Next>(n)( std::forward<Func>(f)( std::forward<Args>(args)... ) ) )
{
	return std::forward<Next>(n)( std::forward<Func>(f)( std::forward<Args>(args)... ) );
}

template<class Func, class Next, class... Args>
auto chain_invoke_impl( Func&& f, Next&& n, std::false_type, Args&&... args )
	-> decltype( invoke( std::forward<Next>(n) ) )
{
	invoke( std::forward<Func>(f), std::forward<Args>(args)... );
	return invoke( std::forward<Next>(n) );
}

template<class Func, class Next, class... Args>
auto chain_invoke( Func&& f, Next&& n, Args&&... args )
	-> decltype( chain_invoke_impl( std::forward<Func>(f),
	                                std::forward<Next>(n),
	                                typename IsCallableWith<
	                                typename std::remove_reference<Next>::type::func_type,
	                                decltype( std::declval<Func>()( std::declval<Args>()... ) )
	                                >::type{},
	                                std::forward<Args>(args)... ) )
{
	return chain_invoke_impl( std::forward<Func>(f),
	                          std::forward<Next>(n),
	                          typename IsCallableWith<
	                          typename std::remove_reference<Next>::type::func_type,
	                          decltype( std::declval<Func>()( std::declval<Args>()... ) )
	                          >::type{},
	                          std::forward<Args>(args)... );
}

template<class Func>
struct invocation
{
	typedef typename convert_functor<Func>::type func_type;

	func_type func;

	template<class F>
	explicit invocation(F&& f)
		: func( std::forward<F>(f) )
	{}

	invocation(invocation const&) = default;
	invocation(invocation&&) = default;

	template<class... A>
	auto invoke(A&&... args)
		-> decltype( ::as::invoke( func, std::forward<A>(args)... ) )
	{
		return ::as::invoke( func, std::forward<A>(args)... );
	}

	template<class... A>
	auto operator()(A&&... args)
		-> decltype( this->invoke( std::forward<A>(args)... ) )
	{
		return this->invoke( std::forward<A>(args)... );
	}
};

template<class Func, class... Args>
struct full_invocation
{
	typedef std::tuple< typename std::decay<Args>::type...> arg_tuple_type;

private:
	invocation<Func> inv;
	arg_tuple_type arg_tuple;

	static constexpr int TSize = std::tuple_size< arg_tuple_type >::value;

public:
	template<class F, class... A>
	explicit full_invocation(F&& f, A&&... args)
		: inv( std::forward<F>(f) )
		, arg_tuple( std::forward<A>(args)... )
	{}

	auto invoke()
		-> decltype( tuple_invoke( inv, arg_tuple ) )
	{
		return ::as::tuple_invoke( inv, arg_tuple );
	}

	auto operator()()
		-> decltype( this->invoke() )
	{
		return this->invoke();
	}
};

template<class... Invokers>
struct chain_invocation;

template<class First, class Second, class... Invokers>
struct chain_invocation<First, Second, Invokers...>
	: chain_invocation<Second, Invokers...>
{
	typedef chain_invocation<Second, Invokers...> base_type;

	First inv;

	template<class F, class S, class... Is>
	chain_invocation(F&& i1, S&& i2, Is&&... invks)
		: base_type( std::forward<S>(i2), std::forward<Is>(invks)... )
		, inv( std::forward<F>(i1) )
	{}

public:
	template<class... Args>
	auto invoke(Args&&... args)
		-> decltype( chain_invoke( inv, std::declval<base_type>().inv, std::forward<Args>(args)... ) )
	{
		return chain_invoke( inv, base_type::inv, std::forward<Args>(args)... );
	}

	template<class... Args>
	auto operator()(Args&&... args)
		-> decltype( this->invoke( std::forward<Args>(args)... ) )
	{
		return this->invoke( std::forward<Args>(args)... );
	}
};

// TODO: this specialization is awkward and was difficult to get
// right; it needs to be initialized with a fully ready
// full_invocation or invocation
template<class First>
struct chain_invocation<First>
{
	First inv;

	explicit chain_invocation(First&& f)
		: inv(std::move(f))
	{}

	template<class... Args>
	auto invoke(Args&&... args)
		-> decltype( inv.invoke( std::forward<Args>(args)... ) )
	{
		return inv.invoke( std::forward<Args>(args)... );
	}

	template<class... Args>
	auto operator()(Args&&... args)
		-> decltype( this->invoke( std::forward<Args>(args)... ) )
	{
		return this->invoke( std::forward<Args>(args)... );
	}
};

template<class Callables, class Args>
struct invoker_builder;

template<class FirstCallable, class... Callables, class... Args>
struct invoker_builder< std::tuple<FirstCallable, Callables...>, std::tuple<Args...> >
{
	typedef full_invocation<FirstCallable, Args...> inv1_type;

	typedef chain_invocation< inv1_type, invocation<Callables>... >  chain_type;
	typedef chain_type result_type;

	template<class... Cs, class... A>
	static chain_type build_chain(inv1_type&& c1, Cs&&... cs)
	{
		return chain_type( std::move(c1), invocation<Callables>{ std::forward<Cs>(cs) }... );
	}

	template<class F, class... A>
	static result_type build(F&& c, Callables const&... cs, A&&... args)
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

	Func func;
	Exec *executor;

	template<class F>
	PostTask(Exec *ex, F&& f)
		: func( std::forward<F>(f) )
		, executor(ex)
	{}

	TaskStatus Invoke()
	{
		func();

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
