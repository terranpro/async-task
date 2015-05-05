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

// TODO: helper for SFINAE
template<class T>
struct void_type
{
	typedef void type;
};

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

template<class T, T>
struct ClassCallableEnabler
{
	typedef void enable_type;
};

template<class T, class Enable = void>
struct ClassFunctionSignatureHelper
{};

template<class T>
struct ClassFunctionSignatureHelper<T, typename ClassCallableEnabler<decltype(&T::operator()), &T::operator()>::enable_type >
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
	typedef std::true_type yes;
	typedef std::false_type no;

	template<class T>
	static yes check( typename ClassFunctionSignatureHelper<T>::type );

	template<class>
	static no check(...);

	static constexpr bool value = std::is_same< decltype(check<C>( 0 )), yes >::value;
	typedef decltype( check<C>(0) ) type;
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

// IsCallable with Signature/Args
template<class C, class... Args>
struct IsCallableWithClassHelper
{
	typedef std::true_type yes;
	typedef std::false_type no;

	template<class U, U> struct check_helper;

	template<class T>
	static auto check( int ) -> decltype( std::declval<T>()( std::declval<Args>()... ), yes() );

	template<class>
	static no check(...);

	static constexpr bool value = std::is_same< decltype(check<C>( 0 )), std::true_type >::value;
	typedef decltype( check<C>(0) ) type;
};

template<class C, class R, class... Args>
struct IsCallableWithClassHelper<C, R(Args...)>
{
	typedef std::true_type yes;
	typedef std::false_type no;

	template<class U, U> struct check_helper;

	template<class T>
	static auto check( int ) -> decltype( std::declval<T>()( std::declval<Args>()... ), yes() );

	template<class>
	static no check(...);

	static constexpr bool value = std::is_same< decltype(check<C>( 0 )), std::true_type >::value;
	typedef decltype( check<C>(0) ) type;
};

template<class T, class... Args>
struct IsCallableWithFunctionHelper
{
	typedef std::true_type yes;
	typedef std::false_type no;

	template<class U>
	static auto check( int ) -> decltype( std::declval<U>()( std::declval<Args>()... ), yes() );

	template<class U>
	static no check(...);

	static constexpr bool value = std::is_same< decltype(check<T>(0)), std::true_type >::value;
	typedef decltype( check<T>(0) ) type;
};

template<class T, class R, class... Args>
struct IsCallableWithFunctionHelper<T, R(Args...)>
{
	typedef std::true_type yes;
	typedef std::false_type no;

	template<class U>
	static auto check( int ) -> decltype( std::declval<U>()( std::declval<Args>()... ), yes() );

	template<class U>
	static no check(...);

	static constexpr bool value = std::is_same< decltype(check<T>(0)), std::true_type >::value;
	typedef decltype( check<T>(0) ) type;
};

template<class T, class... Args>
struct IsCallableWith
	: std::conditional< std::is_class<T>::value,
	                    IsCallableWithClassHelper<T, Args...>,
	                    IsCallableWithFunctionHelper<T, Args...> >::type
{};

template<class T, class Enable = void>
struct HasArgHelper
	: std::false_type
{};

template<class T>
struct HasArgHelper<T, typename void_type<typename FunctionSignature<T>::arg1_type>::type >
	: std::true_type
{};

template<class T, class Enable = void>
struct HasArg : std::false_type
{};

template<class T>
struct HasArg<T, typename std::enable_if< IsCallable<T>::value >::type >
	: HasArgHelper<T>
{};

// Split a list of types into tuple of callables and a tuple of args
template< template<class> class Pred, class... Args>
struct SplitBy;

template< template<class> class Pred, class Next, class... Args>
struct SplitBy< Pred, Next, Args...>
	: std::conditional< Pred<Next>::value,
	                    SplitBy< Pred, std::tuple<Next>, Args... >,
	                    SplitBy< Pred, std::tuple<>, std::tuple<Next, Args...> > >::type
{};

template< template<class> class Pred, class... TrueTypes, class Next, class... Args>
struct SplitBy< Pred, std::tuple<TrueTypes...>, Next, Args... >
	: std::conditional< Pred<Next>::value,
	                    SplitBy< Pred, std::tuple<TrueTypes..., Next>, Args... >,
	                    SplitBy< Pred, std::tuple<TrueTypes...>, std::tuple< Next, Args... > >
        >::type
{};

template< template<class> class Pred, class... TrueTypes>
struct SplitBy< Pred, std::tuple<TrueTypes...> >
	: SplitBy< Pred, std::tuple<TrueTypes...>, std::tuple<> >
{};

template< template<class> class Pred, class... TrueTypes, class... FalseTypes>
struct SplitBy< Pred, std::tuple<TrueTypes...>, std::tuple<FalseTypes...> >
{
	typedef std::tuple<TrueTypes...> true_types;
	typedef std::tuple<FalseTypes...> false_types;
};

template< template<class> class Pred>
struct SplitBy<Pred>
{
	typedef std::tuple<> type;
	typedef std::tuple<> args;
};

} // namespace as

#undef BUILD_ARG_TYPE

#endif // AS_CALLABLE_TRAITS_HPP
