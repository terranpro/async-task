#include "Async.hpp"

#include <iostream>

namespace {
const unsigned int iterations = 1000000;
std::atomic<int> function_count(0);
}

void function1()
{
  ++function_count;
}

void thread_work_test()
{
	as::invocation<decltype(function1)> invoker( function1 );

	auto task = as::PostTask<as::ThreadExecutor, decltype(invoker)>( NULL, std::move(invoker) );
	as::ThreadWork *work = new as::ThreadWorkImpl< decltype(task) >( std::move(task) );

	const int chains = 4;

	function_count = 0;

	using clock = std::chrono::high_resolution_clock;
	clock::time_point start = clock::now();
	{
		for( int c = 0; c < chains; ++c )
			for( int i = 0; i < iterations; ++i )
				(*work)();
	}
  clock::duration elapsed = clock::now() - start;

  std::cout << "time per switch: ";
  clock::duration per_iteration = elapsed / iterations / chains;
  std::cout << std::chrono::duration_cast<std::chrono::nanoseconds>(per_iteration).count() << " ns\n";

  assert( function_count == iterations * chains );
}

int main(int argc, char *argv[])
{
	thread_work_test();

	return 0;
}
