#include "Async.hpp"
// #include "Sync.hpp"
//#include "Await.hpp"
// #include "AsyncPtr.hpp"
#include "ThreadExecutor.hpp"
//#include "GlibExecutor.hpp"

#include <iostream>
#include <fstream>

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

	foo(foo&&) = default;

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

// template<class T>
// struct TaskFinisher
// {
// 	as::TaskFuture<T> result;

// 	TaskFinisher(as::TaskFuture<T> result)
// 		: result(result)
// 	{}

// 	TaskFinisher(const TaskFinisher&) = default;
// 	TaskFinisher(TaskFinisher&&) = default;

// 	~TaskFinisher()
// 	{
// 		if (result.Valid())
// 			result.Get();
// 	}
// };

/*
void foo_test()
{
	const int init_val = 31337;

	try {


		// as::AsyncPtr<foo> handle
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

	mega_work_r.Get();

#endif // AS_USE_COROUTINE_TASKS
}

void thread_executor_test()
{
	bool inner_done = false;

	as::ThreadExecutor exec;

	as::TaskFuture<void> mid_result;
	as::TaskFuture<void> inner_result;

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
		auto p = as::make_task_pair( as::Task::GenericTag{}, middlefunc );
		mid_result = p.second;

		exec.ScheduleAfter( p.first, std::chrono::seconds(1) );
	};

	auto pouter = as::make_task_pair( as::Task::GenericTag{}, outerfunc );

	exec.ScheduleAfter( pouter.first, std::chrono::seconds(2) );

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

	inner_task.Cancel();

	std::this_thread::sleep_for( std::chrono::seconds(1) );

	finishers.clear();
	assert( counter == iters );

	pouter.second.Get();

	mid_result.Get();

	//inner_result.Get();

	//assert( inner_done == true );
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

	as::AsyncPtr<foo> foo_aptr2 = as::async( []() {
			return std::unique_ptr<foo>( new foo(42) );
		} );

	foo_aptr2->inc();

	assert( foo_aptr2->x == 43 );
}

struct base {
	virtual ~base() {}
	virtual void action()
	{
		std::cout << "base!\n";
	}
	virtual void pure_virt() = 0;
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

	virtual void pure_virt() override
	{}

	template<class Func, class... Args>
	void run_func(Func&& func, Args&&... args)
	{
		func(args...);
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

	// base loaded from base unique_ptr created by factory func
	auto factory_func = []() {
		return std::unique_ptr<base>( new child );
	};
	as::AsyncPtr<base> aptr12 = as::async( [=]() {
			return factory_func();
		} );

	as::ThreadExecutor e;
	auto aptr13 = as::make_async<base>( e, factory_func );
	aptr13->action();
}

void async_ptr_recursive_use_test()
{
	as::AsyncPtr<child> aptr = as::make_async<child>();

	aptr->run_func( [=]() {
			aptr->action();
			std::cout << "Action done!\n";
	} );
}

void async_ops_test()
{
	const int THREAD_COUNT = 128;
	child c;
	{
		as::ThreadExecutor child_ctxt;

		std::vector< TaskFinisher<void> > results;
		for ( auto i = 0; i < THREAD_COUNT; ++i ) {
			auto r = as::async( [&]() {
					as::async( child_ctxt, [&c]() {
							c.action();
						} );
				} );

			results.emplace_back( r );
		}

		results.clear();
	}

	assert( c.actions == ( THREAD_COUNT ) );
}

void repeated_task_test()
{
	as::ThreadExecutor ex;
	int x = 0;

	auto tp =
		as::make_task_pair( as::Task::GenericTag{},
			[&x]() -> as::TaskResult<void>
			{
				++x;

				std::cout << "Task Is Run: " << x << "\n";

				if ( x < 5 )
					return as::continuing();

				return as::finished();
			} );

	ex.ScheduleAfter( tp.first, std::chrono::seconds(1) );

	while( tp.second.Get() ) {
		std::cout << "Got Result: " << "\n";
	}

	assert( x == 5 );
}

void pipeline_simulation()
{
	as::ThreadExecutor t_stage1;
	as::ThreadExecutor t_stage2;

	auto func_stage1 = [](std::istream& is) -> as::TaskResult<std::string>
		{
			std::string in;
			while( std::getline(is, in) ) {
				std::cout << "got txt: " << in << "\n";
				return as::continuing( in );
			}

			return as::cancel;
		};

	auto func_stage2 = [](as::TaskFuture< as::TaskResult<std::string> > res)
		-> as::TaskResult<int>
		{
			std::cout << "Stage2 Get PRE\n";
			auto in = res.Get();
			std::cout << "Stage2 Get POST\n";

			if ( !in )
				return as::cancel;

			int s = in->size();

			return as::continuing( s );
		};

	auto stage1_res = as::async( func_stage1, std::ref(std::cin) );

	auto stage2_res = as::async( t_stage2, func_stage2, stage1_res );

	while( std::unique_ptr<int> out = stage2_res.Get() ) {
		std::cout << "Got output: " << *out << "\n";
	}
}

void sync_test()
{
	auto x = as::sync( []() {
			return 42;
		} );

	assert( x == 42 );

	auto y = as::sync( as::Executor::GetDefault(), []() {
			std::this_thread::sleep_for( std::chrono::milliseconds(500) );

			auto z = as::sync( as::Executor::GetDefault(), []() {
					std::this_thread::sleep_for( std::chrono::milliseconds(50) );
					return -1;
				} );

			assert( z == -1 );

			return z + 99;
		} );

	assert( y == 98 );
}

*/

const unsigned int iterations = 1000000;
//const unsigned int iterations = 100;

void chain(as::ThreadExecutor& ex, unsigned int i)
{
	if (i < iterations)
		as::async( ex, chain, std::ref(ex), i + 1 );
}

void post_chain(as::ThreadExecutor ex, unsigned int i)
{
	if (i < iterations)
		as::post( ex, [ex, i]{ post_chain(ex, i + 1); } );
}

as::TaskResult<void>
channel_chain(as::ThreadExecutor ex, unsigned int i)
{
	if (i < iterations)
		as::async( ex, channel_chain, ex, i + 1);

	return as::cancel;
}

void function_context_switch_test()
{
	const int chains = 4;

	using clock = std::chrono::high_resolution_clock;

	as::ThreadExecutor ex( "testing" );
	for( int i = 0; i < chains; ++i )
		as::async( ex, [&]() { chain(ex, 0); } );

	clock::time_point start = clock::now();
	{
		ex.Run();
	}
  clock::duration elapsed = clock::now() - start;

  std::cout << "time per switch: ";
  clock::duration per_iteration = elapsed / iterations / chains;
  std::cout << std::chrono::duration_cast<std::chrono::nanoseconds>(per_iteration).count() << " ns\n";
  std::cout << "switches per second: ";
  std::cout << (std::chrono::seconds(1) / per_iteration) << "\n";
}

void post_test()
{
	const int chains = 4;

	using clock = std::chrono::high_resolution_clock;

	as::ThreadExecutor ex( "testing" );

	for( int i = 0; i < chains; ++i )
		as::post( ex, post_chain, ex, 0 );

	// as::post( ex, []() { return foo(99); },
	//           [](foo i) { std::cout << i << " \n"; } );

	// as::post( ex, []() { return 99; },
	//           [](int&& x) { std::cout << x << "finished\n"; },
	//           []() { std::cout << "amazing!\n"; }
	//           , []() { std::cout << "amazing!\n"; }
	//           , []() { std::cout << "amazing!\n"; }
	//           , []() { std::cout << "amazing!\n"; }
	//           , []() { std::cout << "amazing!\n"; }
	//         );

	clock::time_point start = clock::now();
	{
		ex.Run();
	}
  clock::duration elapsed = clock::now() - start;

  std::cout << "time per switch: ";
  clock::duration per_iteration = elapsed / iterations / chains;
  std::cout << std::chrono::duration_cast<std::chrono::nanoseconds>(per_iteration).count() << " ns\n";
  std::cout << "switches per second: ";
  std::cout << (std::chrono::seconds(1) / per_iteration) << "\n";
}

// std::atomic<int> function_count(0);

// void function1()
// {
//   ++function_count;
// }

// void thread_work_test()
// {
// 	auto invoker = as::PostInvoker<as::ThreadExecutor>( function1 );
// 	as::ThreadWork *work = new as::ThreadWorkImpl< decltype(invoker) >( std::move(invoker) );

// 	const int chains = 4;

// 	function_count = 0;

// 	using clock = std::chrono::high_resolution_clock;
// 	clock::time_point start = clock::now();
// 	{
// 		for( int c = 0; c < chains; ++c )
// 			for( int i = 0; i < iterations; ++i )
// 				(*work)();
// 	}
//   clock::duration elapsed = clock::now() - start;

//   std::cout << "time per switch: ";
//   clock::duration per_iteration = elapsed / iterations / chains;
//   std::cout << std::chrono::duration_cast<std::chrono::nanoseconds>(per_iteration).count() << " ns\n";

//   assert( function_count == iterations * chains );
// }

int main(int argc, char *argv[])
{
	// foo_test();

	// coro_test();

	// thread_executor_test();

	// async_ptr_from_unique_ptr();

	// async_ptr_init_base_from_child_test();

	// async_ptr_recursive_use_test();

	// async_ops_test();

	// repeated_task_test();

	// pipeline_simulation();

	// sync_test();

	function_context_switch_test();

	post_test();

	// invoker_test();

	// thread_work_test();

	return 0;
}
