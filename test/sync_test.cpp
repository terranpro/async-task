#include "Sync.hpp"

#include <cassert>

int main(int argc, char *argv[])
{
	auto r = as::sync( []() {
			std::this_thread::sleep_for( std::chrono::seconds(5) );
			return 42;
		} );

	assert( r == 42 );

	as::ThreadExecutor ex;

	auto s = as::sync( ex, []() {
			std::this_thread::sleep_for( std::chrono::seconds(2) );
			return 99;
		} );

	assert( s == 99 );

	return 0;
}
