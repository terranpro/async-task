#include "Await.hpp"
#include "ThreadExecutor.hpp"

#include <iostream>
#include <atomic>
#include <chrono>
#include <vector>

#include <cassert>

struct foo
{
	static int obj_cons;
	static std::atomic<int> obj_copy;

	std::atomic<int> x;
	//std::atomic<int> users;
	int users;

	explicit foo(int x)
		: x(x)
		, users{0}
	{
		++obj_cons;
	}

	foo(foo const& other)
		: x(other.x.load())
		, users(0)
	{
		++obj_copy;
	}

	void inc()
	{
		++users;

		assert( users < 2 );

		++x;
		--users;
	}

	foo& operator++()
	{
		++x;
		return *this;
	}

	foo operator++(int)
	{
		++x;
		return *this;
	}

	friend std::ostream& operator<<(std::ostream& os, foo const& f)
	{
		return os << f.x;
	}
};

int foo::obj_cons = 0;
std::atomic<int> foo::obj_copy{0};

template<class T>
struct TaskFinisher
{
	as::TaskResult<T> result;

	TaskFinisher(as::TaskResult<T> result)
		: result(result)
	{}

	TaskFinisher(const TaskFinisher&) = default;
	TaskFinisher(TaskFinisher&&) = default;

	~TaskFinisher()
	{
		if (result.Valid())
			result.Get();
	}
};

void foo_test()
{
	const int init_val = 31337;

	/* as::AsyncPtr<foo> handle */
	auto handle = as::make_async<foo>(
		[init_val]() {
			return foo{init_val};
		} );

	// alternate syntax
	//as::AsyncPtr<foo> handle = as::make_async<foo>( 31337 );

	as::AsyncPtr<int> handle2{ as::make_async<int>( 42 ) };

	constexpr int THREAD_COUNT = 256;

	decltype( std::chrono::high_resolution_clock::now() - std::chrono::high_resolution_clock::now() )
		clock_dur{};

	std::vector< TaskFinisher< decltype(clock_dur) > > finishers;

	// Force initialization to finish, so we measure only access overhead
	handle.Sync();

	for ( int t = 0; t < THREAD_COUNT; ++t ) {

		auto r = as::async( [=]() mutable {

				auto beg = std::chrono::high_resolution_clock::now();
				handle->inc();
				auto end = std::chrono::high_resolution_clock::now();

				return end-beg;
			} );

		finishers.emplace_back( r );
	}

	for ( auto& f : finishers ) {
		auto dur = f.result.Get();
		clock_dur += dur;
	}

	{
		// must destroy proxy to release the lock!
		auto proxy = handle2.GetProxy();
		*proxy = 96;
	}

	auto xyz = handle2;

	assert( handle );
	assert( handle2 );

	assert( handle->x == init_val + THREAD_COUNT );
	assert( *handle2 == 96 );
	assert( *handle2 == xyz );

	std::cout << "foo cons: " << foo::obj_cons << "\n";
	std::cout << "foo copy: " << foo::obj_copy << "\n";

	auto dur_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(clock_dur);

	std::cout << "Clock duration: " << dur_ns.count() << "\n";

	finishers.clear();
}

void coro_test()
{
#ifdef AS_USE_COROUTINE_TASKS

	auto& ctxt = as::GlibExecutor::GetDefault();

	auto mega_work_r =
		as::async( ctxt, [&]() {
				int x = 50;
				std::cout << "Doing mega work\n";
				while( x-- ) {
					std::cout << ".";
					std::cout.flush();
					std::this_thread::sleep_for( std::chrono::milliseconds(1) );
					as::ThisTask::Yield();
				}
			} );

	as::await( ctxt, [&]() {
			std::cout << "Awaiting...!\n";

			as::ThisTask::Yield();

			as::await( []() {
					std::cout << "Start sleep...\n";
					std::this_thread::sleep_for( std::chrono::seconds(5) );
					std::cout << "Done!\n";
			} );

			std::cout << "Awaiting DONE...!\n";
	} );

#endif // AS_USE_COROUTINE_TASKS
}

void thread_executor_test()
{
	as::ThreadExecutor exec;

	exec.ScheduleAfter( [&]() {
			std::cout << "Amazing forever.\n";
			exec.ScheduleAfter( [&]() {
					std::cout << "First!\n";
					exec.ScheduleAfter( []() {
							std::cout << "Inner!\n";
						}, std::chrono::seconds(1) );

				}, std::chrono::seconds(1) );

		}, std::chrono::seconds(2) );

	int counter = 0;
	int iters = 969;

	std::vector< TaskFinisher<void> > finishers;

	for ( int i = 0; i < iters; ++i ) {
		finishers.emplace_back(
			as::async( exec,
			           [&counter]() {
				           ++counter;
			           } )
		                      );
	}

	std::this_thread::sleep_for( std::chrono::seconds(5) );

	finishers.clear();
	assert( counter == iters );
}

int main(int argc, char *argv[])
{
	foo_test();

	coro_test();

	thread_executor_test();

	return 0;
}
