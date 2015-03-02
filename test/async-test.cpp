#include "Async.hpp"
#include "Await.hpp"
#include "ThreadExecutor.hpp"
#include "GlibExecutor.hpp"

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

	try {


		/* as::AsyncPtr<foo> handle */
		auto handle = as::make_async<foo>(
			[init_val]() {
				return foo{init_val};
			} );

		// alternate syntax
		//as::AsyncPtr<foo> handle = as::make_async<foo>( 31337 );

		as::AsyncPtr<int> handle2{ as::make_async<int>( 42 ) };

		constexpr int THREAD_COUNT = 128;

		decltype( std::chrono::high_resolution_clock::now() - std::chrono::high_resolution_clock::now() )
			clock_dur{};

		std::vector< TaskFinisher< decltype(clock_dur) > > finishers;

		// Force initialization to finish, so we measure only access overhead
		handle.Sync();

		std::vector< as::ThreadExecutor > threads(THREAD_COUNT);

		for ( int t = 0; t < THREAD_COUNT; ++t ) {

			auto r = as::async( threads[0], [=]() mutable {

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

		auto dur_ns = std::chrono::duration_cast<std::chrono::microseconds>(clock_dur);

		std::cout << "Clock duration: " << dur_ns.count() << "\n";

		finishers.clear();

	} catch (std::exception& ex) {
		std::cout << "Caught exception: ex.what()\n";
		throw;
	}

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
					std::this_thread::sleep_for( std::chrono::seconds(2) );
					std::cout << "Done!\n";
			} );

			std::cout << "Awaiting DONE...!\n";
	} );

	mega_work_r.Get();

#endif // AS_USE_COROUTINE_TASKS
}

void thread_executor_test()
{
	bool inner_done = false;

	as::ThreadExecutor exec;

	as::TaskResult<void> mid_result;
	as::TaskResult<void> inner_result;

	auto innerfunc = [&inner_done]() {
		std::cout << "Inner!\n";
		inner_done = true;
	};

	auto pair = as::make_task_pair( as::Task::GenericTag{}, innerfunc );
	auto inner_task = pair.first;
	inner_result = pair.second;

	auto middlefunc = [&]() {
		std::cout << "First!\n";

		exec.ScheduleAfter( inner_task, std::chrono::seconds(1) );
	};

	auto outerfunc = [&]() {
		std::cout << "Amazing forever.\n";
		exec.ScheduleAfter( middlefunc, std::chrono::seconds(1) );
	};

	exec.ScheduleAfter( outerfunc, std::chrono::seconds(2) );

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

	std::this_thread::sleep_for( std::chrono::seconds(1) );

	finishers.clear();
	assert( counter == iters );

	inner_result.Get();

	assert( inner_done == true );
}

void async_ptr_from_unique_ptr()
{
	std::unique_ptr<foo> foo_uptr{new foo(42)};

	as::AsyncPtr<foo> foo_aptr( std::move(foo_uptr) );

	assert( foo_aptr );
	assert( foo_aptr->x == 42 );

	foo_aptr->inc();

	assert( foo_aptr->x == 43 );

	assert( !foo_uptr );
}

struct base {
	virtual ~base() {}
	virtual void action()
	{
		std::cout << "base!\n";
	}
};
struct child : public base
{
	int users;
	int actions;

	child(child const& other)
		: users(0)
		, actions(0)
	{}

	child()
		: users(0)
		, actions(0)
	{}

	virtual void action() override
	{
		++users;
		++actions;

		assert( users == 1 );

		--users;
	}
};

void async_ptr_init_base_from_child_test()
{
	as::AsyncPtr<base> aptr = as::make_async<child>();

	assert( aptr );

	aptr->action();

	// build from returning object
	as::AsyncPtr<base> aptr2 = as::async( []() {
			return child();
		} );

	assert( aptr2 );

	aptr2->action();

	// build from returning object pointer
	as::AsyncPtr<base> aptr3 = as::async( []() {
			return new child;
		} );

	aptr3->action();

	// move an asyncptr and assure it still works
	as::AsyncPtr<base> aptr4 = as::make_async<child>();
	auto aptr5 = std::move( aptr4 );

	assert( aptr5 );
	assert( !aptr4 );

	aptr5->action();

	// move an asyncptr and asure it still works (async() version)
	as::AsyncPtr<base> aptr6 = as::async([]() {
			return new child;
		} );

	auto aptr7 = std::move(aptr6);
	assert( aptr7 );
	assert( !aptr6 );
	aptr7->action();

	as::AsyncPtr<base> aptr8 = as::async([]() {
			return child();
		} );
	auto aptr9 = std::move(aptr8);

	assert( aptr9 );
	assert( !aptr8 );

	aptr9->action();

	// copy an asyncptr of derived to base and use both
	as::AsyncPtr<child> aptr10 = as::make_async<child>();
	as::AsyncPtr<base> aptr11 = aptr10;

	assert( aptr10 );
	assert( aptr11 );

	aptr10->action();
	aptr11->action();

	assert( aptr10->actions == 2 );

	// now a lot in a bunch of threads
	std::vector< TaskFinisher<void> > results;

	const int THREAD_COUNT = 64;
	for ( auto i = 0; i < THREAD_COUNT; ++i ) {
		auto r = as::async( [aptr10]() {
				aptr10->action();
			} );
		results.emplace_back( r );

		r = as::async( [aptr11]() {
				aptr11->action();
			} );
		results.emplace_back( r );
	}
	results.clear();

	assert( aptr10->actions == ( THREAD_COUNT*2 + 2 ) );
}

int main(int argc, char *argv[])
{
	foo_test();

	coro_test();

	thread_executor_test();

	async_ptr_from_unique_ptr();

	async_ptr_init_base_from_child_test();

	return 0;
}