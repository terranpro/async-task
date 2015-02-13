#include "Async.hpp"

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
		std::cout << "GetRekt and Erekt.\n";
	}

	foo(foo const& other)
		: x(other.x.load())
		, users(0)
	{
		++obj_copy;
		std::cout << "other x: " << x << "\n";
	}

	~foo()
	{
		std::cout << "x: " << x << "\n";
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
	// TaskFinisher(as::TaskResult<T>&& result)
	// 	: result(std::move(result))
	// {}
	~TaskFinisher()
	{
		result.Get();
	}
};

void foo_test()
{
	auto result = as::async([]() {
			//std::this_thread::sleep_for( std::chrono::seconds(2) );

			std::cout << "Get rekt, and Erekt plz.\n";

			return foo{31337};
		} );

	as::AsyncHandle<foo> handle{ result };

	as::AsyncHandle<int> handle2{
		as::async([]()
		          {
			          //std::this_thread::sleep_for( std::chrono::seconds(1) );
			          return 42;
		          } )
			};

	constexpr int THREAD_COUNT = 256;

	decltype( std::chrono::high_resolution_clock::now() - std::chrono::high_resolution_clock::now() )
		clock_dur{};

	result.Get();

	std::vector< TaskFinisher< decltype(clock_dur) > > finishers;

	for ( int t = 0; t < THREAD_COUNT; ++t ) {

		auto r = as::async( [=]() mutable {
				//std::this_thread::sleep_for( std::chrono::microseconds(100) );

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

	int xyz = handle2;

	assert( handle );
	assert( handle2 );

	std::cout << "Handle value: " << handle << "\n";
	std::cout << "Handle<int> value: " << *handle2 << "\n";
	std::cout << "xyz value: " << xyz << "\n";

	std::cout << "foo cons: " << foo::obj_cons << "\n";
	std::cout << "foo copy: " << foo::obj_copy << "\n";

	auto dur_ms = std::chrono::duration_cast<std::chrono::microseconds>(clock_dur);

	std::cout << "Clock duration: " << dur_ms.count() << "\n";

	finishers.clear();
}

void coro_test()
{
	as::Task t1(
		[]() {
			std::cout << "t1 started!\n";

			as::ThisTask::Yield();

			std::cout << "t1 finished!\n";
		} );

	as::Task c1{ as::CoroutineTaskTag{},
		[]() {
			std::cout << "c1 started!\n";

			as::ThisTask::Yield();

			std::cout << "c1 finished!\n";
		}
	};

	as::Task c2{ as::CoroutineTaskTag{},
		[t1]() mutable {
			std::cout << "c2 started!\n";

			t1.Invoke();

			as::ThisTask::Yield();

			std::cout << "c2 finished!\n";
		}
	};

	as::Task c3{ as::CoroutineTaskTag{},
		[t1]() mutable {

			as::ThisTask::Yield();

			t1.Invoke();

			return 3.141526;
		}
	};

	auto& ctxt = as::GlibExecutionContext::GetDefault();
	ctxt.AddTask( c2 );
	ctxt.AddTask( c3 );

	ctxt.Iteration();
	std::cout << "!!!!\n";
	ctxt.Iteration();
	std::cout << "!!!!\n";
	ctxt.Iteration();
	std::cout << "!!!!\n";
	ctxt.Iteration();
}


int main(int argc, char *argv[])
{
	foo_test();

	coro_test();

	return 0;
}
