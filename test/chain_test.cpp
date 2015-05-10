#include "Async.hpp"

#include <iostream>

int f()
{return 42;}

int g(int i)
{return i - 9;}

int h(int j)
{return j*2;}

void print(int k)
{std::cout << k << "\n";}

int main(int argc, char *argv[])
{
	as::ThreadExecutor ex{"haha"};
	as::ThreadExecutor ex2{"hehe"};

	as::post( ex, f, as::bind( ex2, print, std::placeholders::_1 ) );

	as::post( ex,
	          f, g, h,
	          as::bind( ex2, print, std::placeholders::_1 )
	        );

	ex.Run();
	ex2.Run();

	return 0;
}
