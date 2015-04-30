//
//  CallableTraits.hpp - Utility classes to determine signature of a
//  function/functor/lambda
//
//  Copyright (c) 2015 Brian Fransioli
//
//  Distributed under the Boost Software License, Version 1.0.
//  See accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt
//

#ifndef AS_CALLABLE_TRAITS_HPP
#define AS_CALLABLE_TRAITS_HPP

#include <type_traits>
#include <typeinfo>
#include <tuple>

#define BUILD_ARG_TYPE(N)	  \
	template<class T> \
	struct ArgType<T, N> \
	{ \
		typedef T arg ## N ##_type; \
	};

namespace as {

template<class T, int N>
struct ArgType;

BUILD_ARG_TYPE(1)
BUILD_ARG_TYPE(2)
BUILD_ARG_TYPE(3)
BUILD_ARG_TYPE(4)
BUILD_ARG_TYPE(5)
BUILD_ARG_TYPE(6)
BUILD_ARG_TYPE(8)
BUILD_ARG_TYPE(9)
BUILD_ARG_TYPE(10)
BUILD_ARG_TYPE(11)
BUILD_ARG_TYPE(12)

template<class Ret, int ArgN, class... Args>
struct ArgSignatureHelper;

template<class Ret, int ArgN>
struct ArgSignatureHelper<Ret, ArgN>
{};

template<class Ret, int ArgN, class Arg, class... Args>
struct ArgSignatureHelper<Ret, ArgN, Arg, Args...>
	: public ArgSignatureHelper< Ret, ArgN+1, Args... >
	, public ArgType<Arg, ArgN>
{};

template<class Signature>
struct ArgSignature;

template<class Ret>
struct ArgSignature<Ret()>
	: public ArgSignatureHelper<Ret, 0>
{};

template<class Ret, class... Args>
struct ArgSignature<Ret(Args...)>
	: public ArgSignatureHelper<Ret, 1, Args...>
{};

template<class Signature>
struct FunctionSignatureIdentity
{
	typedef Signature type;
};

template<class Signature>
struct FunctionSignatureBase;

template<class R, class... Args>
struct FunctionSignatureBase<R(Args...)>
	: FunctionSignatureIdentity<R(Args...)>
	, ArgSignature<R(Args...)>
{
	typedef R return_type;
	typedef std::tuple<Args...> arg_tuple_type;
};

template<class Signature>
struct FunctionSignatureHelper;

template<class R, class... Args>
struct FunctionSignatureHelper<R(Args...)>
	: public FunctionSignatureBase<R(Args...)>
{};

template<class R, class... Args>
struct FunctionSignatureHelper<R(*)(Args...)>
	: public FunctionSignatureBase<R(Args...)>
{};

template<class R, class... Args>
struct FunctionSignatureHelper<R(&)(Args...)>
	: public FunctionSignatureBase<R(Args...)>
{};

template<class R, class C, class... Args>
struct FunctionSignatureHelper<R(C::*)(Args...)>
	: public FunctionSignatureBase<R(Args...)>
{};

template<class R, class C, class... Args>
struct FunctionSignatureHelper<R(C::*)(Args...) const>
	: public FunctionSignatureBase<R(Args...)>
{};

template<class R, class C, class... Args>
struct FunctionSignatureHelper<R(C::*)(Args...) &&>
	: public FunctionSignatureBase<R(Args...)>
{};

template<class R, class C, class... Args>
struct FunctionSignatureHelper<R(C::*)(Args...) const &&>
	: public FunctionSignatureBase<R(Args...)>
{};

template<class T>
struct ClassFunctionSignatureHelper
	: FunctionSignatureHelper< decltype( &T::operator() ) >
{};

template<class T>
struct FunctionSignature
	: std::conditional< std::is_class<T>::value,
	                    ClassFunctionSignatureHelper<T>,
	                    FunctionSignatureHelper< T > >::type
{};

template<class C>
struct IsCallableClassHelper
{
	typedef char (&yes)[2];
	typedef char (& no)[1];

	template<class T>
	static yes check( typename ClassFunctionSignatureHelper<T>::type );

	template<class>
	static no check(...);

	static constexpr bool value = sizeof( check<C>( 0 ) ) == sizeof(yes);
};

template<class Func>
struct IsCallableFunctionHelper
	: std::is_function<
		typename std::remove_pointer< typename std::decay< Func >::type >::type
        >
{};

template<class T>
struct IsCallable
	: std::conditional< std::is_class<T>::value,
	                    IsCallableClassHelper<T>,
	                    IsCallableFunctionHelper<T> >::type
{};

template<class T>
struct HasArgHelper
{ typedef void type; };

template<class T, class Enable = void>
struct HasArg : std::false_type
{};

template<class T>
struct HasArg<T, typename HasArgHelper< typename FunctionSignature<T>::arg1_type >::type >
	: std::true_type
{};

// Split a list of types into tuple of callables and a tuple of args
template<class... Args>
struct SplitByCallable;

template<class Next, class... Args>
struct SplitByCallable< Next, Args...>
	: std::conditional< IsCallable<Next>::value,
	                    SplitByCallable< std::tuple<Next>, Args... >,
	                    SplitByCallable< std::tuple<>, std::tuple<Next, Args...> > >::type
{};

template<class... Callables, class Next, class... Args>
struct SplitByCallable< std::tuple<Callables...>, Next, Args... >
	: std::conditional< IsCallable<Next>::value,
	                    SplitByCallable< std::tuple<Callables..., Next>, Args... >,
	                    SplitByCallable< std::tuple<Callables...>, std::tuple< Next, Args... > >
        >::type
{};

template<class... Callables>
struct SplitByCallable< std::tuple<Callables...> >
	: SplitByCallable< std::tuple<Callables...>, std::tuple<> >
{};

template<class... Callables, class... Args>
struct SplitByCallable< std::tuple<Callables...>, std::tuple<Args...> >
{
	typedef std::tuple<Callables...> type;
	typedef std::tuple<Args...> args;

	//template<class Builder, class C1, class... Cs, class... A>
	template<class Builder, class... A>
	static typename Builder::result_type
	build(Builder&& b, Callables... cs, A&&... args)
	{
		return b( std::make_tuple( std::move(cs)... ),
		          std::forward<A>(args)... );
	}
};

template<>
struct SplitByCallable<>
{
	typedef std::tuple<> type;
	typedef std::tuple<> args;
};

} // namespace as

#undef BUILD_ARG_TYPE

#endif // AS_CALLABLE_TRAITS_HPP
