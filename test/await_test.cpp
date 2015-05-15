#include "Await.hpp"
#include "ThreadExecutor.hpp"

void coro_test()
{
// #ifdef AS_USE_COROUTINE_TASKS

	auto& ctxt = as::ThreadExecutor::GetDefault();

	auto mega_work_r =
		as::async( ctxt, [&]() {
				int x = 50;
				std::cout << "Doing mega work\n";
				while( x-- ) {
					std::cout << ".";
					std::cout.flush();
					std::this_thread::sleep_for( std::chrono::milliseconds(1) );
					as::this_task::yield();
				}
			} );

	as::await( ctxt, [&]() {
			std::cout << "Awaiting...!\n";

			as::this_task::yield();

			as::await( []() {
					std::cout << "Start sleep...\n";
					std::this_thread::sleep_for( std::chrono::seconds(2) );
					std::cout << "Done!\n";
			} );

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
