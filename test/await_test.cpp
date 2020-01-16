#define AS_USE_COROUTINE_TASKS

#include "Await.hpp"
#include "ThreadExecutor.hpp"

void coro_test()
{
// #ifdef AS_USE_COROUTINE_TASKS

	auto& ctxt = as::ThreadExecutor::GetDefault();
	as::ThreadExecutor other_ctxt;

	auto mega_work_r =
		as::await( ctxt, [&]() {
				int x = 50;
				std::cout << "Doing mega work\n";
				while( x-- ) {
					std::cout << "A";
					std::cout.flush();
					std::this_thread::sleep_for( std::chrono::milliseconds(100) );
					as::this_task::yield();
				}
			} );

	as::await( ctxt, [&]() {
			std::cout << "Awaiting...!\n";

			as::this_task::yield();

			auto fut = as::await( ctxt, []() {
					std::cout << "Start sleep...\n";

					for ( auto i = 1; i <= 20; ++i ) {
						std::cout << "B";
						std::cout.flush();
						std::this_thread::sleep_for( std::chrono::milliseconds(100) );
						as::this_task::yield();
					}

					std::cout << "Done!\n";
			} );

			AWAIT( fut );

			std::cout << "Awaiting DONE...!\n";
	} );

	mega_work_r.get();
// #endif // AS_USE_COROUTINE_TASKS

}

int main(int argc, char *argv[])
{
	coro_test();

	return 0;
}
