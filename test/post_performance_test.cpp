#include "Async.hpp"

#include <iostream>

namespace {
unsigned iterations = 1000000;
}

void post_chain(as::ThreadExecutor ex, unsigned int i)
{
	if (i < iterations)
		as::post( ex, [ex, i]{ post_chain(ex, i + 1); } );
}

void post_performance_test()
{
	const int chains = 4;

	using clock = std::chrono::high_resolution_clock;

	as::ThreadExecutor ex( "testing" );

	for( int i = 0; i < chains; ++i )
		as::post( ex, [&]() { post_chain(ex, 0); } );

	clock::time_point start = clock::now();
	{
		ex.Run();
	}
  clock::duration elapsed = clock::now() - start;

  std::cout << "time per switch: ";
  clock::duration per_iteration = elapsed / iterations / chains;
  std::cout << std::chrono::duration_cast<std::chrono::nanoseconds>(per_iteration).count()
            << " ns\n";

  if ( per_iteration.count() != 0 ) {
	  std::cout << "switches per second: ";
	  std::cout << (std::chrono::seconds(1) / per_iteration) << "\n";
  }
}

int main(int argc, char *argv[])
{
	if ( argv[1] )
		iterations = std::stoi(argv[1]);

	post_performance_test();

	return 0;
}
