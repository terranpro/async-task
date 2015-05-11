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
#include <iostream>

template<class T>
void print_type()
{
	std::cout << typeid(T).name() << "\n";
}

void print_stuff()
{}

template<class T, class... Args>
void print_stuff(T t, Args... args)
{
	std::cout << typeid(T).name() << ": " << t << "\n";
	print_stuff( args... );
}

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

template<class Exec, class Func>
struct PostTask
{
	typedef Exec executor_type;
	typedef typename convert_functor<Func>::type function_type;
	// typedef Func function_type;

	function_type func;
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

	Func func;
	Exec *executor;

	AsyncTask(Exec *ex, Func func)
		: func( std::move(func) )
		, executor(ex)
	{}

};

template<class Ex, class Func>
void schedule(Ex&& ex, Func&& f)
{
	std::forward<Ex>(ex).schedule( std::forward<Func>( f ) );
}

template<class Func, class... A>
auto invoke_impl(std::true_type, Func&& f, A&&... args)
	-> decltype( std::forward<Func>(f)( std::forward<A>(args)... ) )
{
	std::cout << __PRETTY_FUNCTION__ << " Func: " << &f << "\n\n\n";

	return std::forward<Func>(f)( std::forward<A>(args)... );
}

template<class Func, class... A>
auto invoke_impl(std::false_type, Func&& f, A&&... args)
	-> decltype( std::forward<Func>(f)() )
{
	print_stuff(args...);
	print_type<Func>();
	std::cout << "Func: " << &f << "\n";

	return std::forward<Func>(f)();
}

template<class Func, class... Args>
auto invoke(Func&& f, Args&&... args)
	-> decltype( invoke_impl( typename IsCallableWith<
	                          typename std::remove_reference<Func>::type,
	                          Args...
	                          >::type{},
	                          std::forward<Func>(f),
	                          std::forward<Args>(args)... ) )
{
	return invoke_impl( typename IsCallableWith<
	                    typename std::remove_reference<Func>::type,
	                    Args...
	                    >::type{},
	                    std::forward<Func>(f),
	                    std::forward<Args>(args)... );
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
void chain_invoke_impl( Func&& f, Next&& n, std::true_type, Args&&... args )
	// -> decltype( std::forward<Next>(n)( std::forward<Func>(f)( std::forward<Args>(args)... ) ) )
{
	//std::forward<Next>(n)( std::forward<Func>(f)( std::forward<Args>(args)... ) );
	invoke( std::forward<Next>(n), invoke( std::forward<Func>(f), std::forward<Args>(args)... ) );
}

template<class Func, class Next, class... Args>
void chain_invoke_impl( Func&& f, Next&& n, std::false_type, Args&&... args )
	// -> decltype( invoke( std::forward<Next>(n) ) )
{
	invoke( std::forward<Func>(f), std::forward<Args>(args)... );
	invoke( std::forward<Next>(n) );
}

template<class Func, class Next, class... Args>
void chain_invoke( Func&& f, Next&& n, Args&&... args )
	// -> decltype( chain_invoke_impl( std::forward<Func>(f),
	//                                 std::forward<Next>(n),
	//                                 typename IsCallableWith<
	//                                 typename std::remove_reference<Next>::type,
	//                                 decltype( std::declval<Func>()( std::declval<Args>()... ) )
	//                                 >::type{},
	//                                 std::forward<Args>(args)... ) )
{
	chain_invoke_impl( std::forward<Func>(f),
	                   std::forward<Next>(n),
	                   typename IsCallableWith<
	                   typename std::remove_reference<Next>::type,
	                   decltype( std::declval<Func>()( std::declval<Args>()... ) )
	                   >::type{},
	                   std::forward<Args>(args)... );
}

template<class Func>
struct invocation
{
	typedef typename convert_functor<Func>::type func_type;

	func_type func;

	explicit invocation(func_type&& f)
		: func( std::move(f) )
	{
		std::cout << __PRETTY_FUNCTION__ << "\n";
	}

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

template<class Ex, class Func>
struct bound_invocation
{
	typedef typename std::remove_reference<Ex>::type executor_type;

	invocation<Func> inv;
	executor_type ex;

	template<class E, class F>
	bound_invocation(E&& e, F&& func)
		: ex( std::forward<E>(e) )
		, inv( std::forward<F>(func) )
	{}
};

template<class Ex, class Func, class... Args>
auto bind(Ex&& ex, Func&& func, Args&&... args)
	-> bound_invocation<Ex, decltype(std::bind(func,args...))>
{
	return { std::forward<Ex>(ex), std::bind( std::forward<Func>(func),
	                                          std::forward<Args>(args)... ) };
}

template<class Ex, class... Invokers>
struct chain_invocation;

template<class Ex, class First, class... Invokers>
struct chain_invocation<Ex, First, Invokers...>
{
	typedef chain_invocation<Ex, Invokers...> base_type;

	invocation<First> inv;
	base_type next;
	Ex ex;

	template<class F, class... Is>
	explicit chain_invocation(Ex ex, F&& i1, Is&&... invks)
		: ex( ex )
		, inv( std::forward<F>(i1) )
		, next( ex, std::forward<Is>(invks)... )
	{}

public:
	template<class... Args>
	void invoke(Args&&... args)
	{
		chain_invoke( inv, next, std::forward<Args>(args)... );
	}

	template<class... Args>
	void operator()(Args&&... args)
	{
		this->invoke( std::forward<Args>(args)... );
	}
};

template<class Ex, class FirstEx, class First, class... Invokers>
struct chain_invocation<Ex, bound_invocation<FirstEx,First>, Invokers...>
{
	typedef chain_invocation<Ex, Invokers...> base_type;
	typedef typename bound_invocation<FirstEx,First>::executor_type executor_type;

	invocation<First> inv;
	base_type next;
	executor_type ex;

	template<class F, class... Is>
	explicit chain_invocation(Ex ex, bound_invocation<FirstEx,F> i1, Is&&... invks)
		: ex( std::move(i1.ex) )
		, inv( std::move(i1.inv) )
		, next( ex, std::forward<Is>(invks)... )
	{}

public:
	template<class... Args>
	void invoke(Args&&... args)
	{
		assert( false );

		chain_invoke( inv, next, std::forward<Args>(args)... );
	}

	template<class... Args>
	void schedule(Args&&... args)
	{
		auto x = std::bind( [](invocation<First> inv, base_type next, Args... a) {
				chain_invoke(inv, next, std::move(a)...);
			}, inv, next, std::forward<Args>(args)... );

		::as::schedule( ex, PostTask<Ex, decltype(x)>( &ex, x ) );
	}

	template<class... Args>
	void operator()(Args&&... args)
	{
		this->schedule( std::forward<Args>(args)... );
	}
};

// TODO: this specialization is awkward and was difficult to get
// right; it needs to be initialized with a fully ready
// full_invocation or invocation
template<class Ex, class First>
struct chain_invocation<Ex, First>
{
	invocation<First> inv;
	Ex ex;

	explicit chain_invocation(Ex ex, First&& f)
		: inv(std::move(f))
		, ex(std::move(ex))
	{}

	template<class... Args>
	void invoke(Args&&... args)
	{
		inv.invoke( std::forward<Args>(args)... );
	}

	template<class... Args>
	void operator()(Args&&... args)
	{
		this->invoke( std::forward<Args>(args)... );
	}
};

template<class Ex, class FirstEx, class First>
struct chain_invocation<Ex, bound_invocation<FirstEx,First>>
{
	typedef typename bound_invocation<FirstEx,First>::executor_type executor_type;

	invocation<First> inv;
	executor_type ex;

	explicit chain_invocation(Ex ex, bound_invocation<FirstEx,First> i1)
		: inv(std::move(i1.inv))
		, ex(std::move(i1.ex))
	{}

	template<class... Args>
	void invoke(Args&&... args)
	{
		assert( false );

		inv.invoke( std::forward<Args>(args)... );
	}

	template<class... Args>
	void schedule(Args&&... args)
	{
		auto x = std::bind( [](invocation<First> inv, Args... a) {
				::as::invoke( inv, std::move(a)... );
			}, inv, std::forward<Args>(args)... );

		::as::schedule( ex, PostTask<Ex, decltype(x)>( &ex, std::move(x) ) );
	}

	template<class... Args>
	void operator()(Args&&... args)
	{
		this->schedule( std::forward<Args>(args)... );
	}
};

template<class Ex, class Func, class... Conts>
auto build_chain(Ex& ex, Func&& func, Conts&&... conts)
	-> chain_invocation<Ex, Func, Conts...>
{
	return chain_invocation<Ex, Func, Conts...>( ex,
	                                             std::forward<Func>(func),
	                                             std::forward<Conts>(conts)... );
}

} // namespace as

#endif // AS_TASK_IMPL_HPP
