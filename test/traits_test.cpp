#include "CallableTraits.hpp"

#include <iostream>
#include <functional>
#include <typeindex>

#include <cassert>

int f()
{return 42;}

int g(int i)
{return i - 9;}

int h(int j)
{return j*2;}

void i(int)
{}

struct foo
{
	int x() {
		return -1;
	}

	int y() const {
		return 22;
	}

	int z(int, int, char) const {}

	int operator()() const {
		return y();
	}
};

template<class Func>
constexpr bool check_callable(Func&& f)
{
	return as::IsCallableWith<Func, void()>::value;
}

template<class Func, class... Args>
constexpr const char *check_result(Func&& f, Args&&... args)
{
	return typeid( typename as::CallableResult<std::tuple<Args...>, Func>::type ).name();
}

template<class Ret, class ArgTuple, class FuncTuple, class Enable = void>
struct check_multi_result_helper
	: std::false_type
{};

template<class Ret, class... Args, class... Funcs>
struct check_multi_result_helper<
	Ret,
	std::tuple<Args...>,
	std::tuple<Funcs...>,
	typename std::enable_if<
		std::is_same< typename as::ChainResultOf<std::tuple<Args...>, Funcs...>::type,
		              Ret >::value >::type
>
	: std::true_type
{};

template<class Ret, class... Args, class... Funcs>
constexpr bool check_multi_result(Funcs&&... funcs)
{
	return check_multi_result_helper<Ret, std::tuple<Args...>, std::tuple<Funcs...> >::value;
}

int main(int argc, char *argv[])
{
	std::cout << check_callable(f) << "\n";
	std::cout << check_callable(std::bind(&foo::x,foo())) << "\n";

	std::cout << check_result( f ) << "\n";
	std::cout << check_result( g, 42 ) << "\n";

	static_assert( check_multi_result<int>(f,g), "" );
	static_assert( check_multi_result<void>(f,g,h,i), "" );
	static_assert( check_multi_result<int>(f,g), "" );
	static_assert( check_multi_result<int>(f,g,h), "" );
	static_assert( check_multi_result<int>(f,g,i,f), "" );

	std::cout << check_callable( std::bind(&foo::z, foo(), 1, 2, 3) ) << "\n";

	std::cout << check_multi_result<void>( std::bind(&foo::z, foo(), 1, 2, 3) ) << "\n";
	std::cout << check_multi_result<int>( std::bind(&foo::z, foo(), 1, 2, 3) ) << "\n";

	return 0;
}
