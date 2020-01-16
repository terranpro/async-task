//
//  CoroutineTaskImpl.hpp - Coroutine Tasks implementation via boost::context
//
//  Copyright (c) 2015 Brian Fransioli
//
//  Distributed under the Boost Software License, Version 1.0.
//  See accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt
//

#ifndef AS_COROUTINE_TASK_IMPL_HPP
#define AS_COROUTINE_TASK_IMPL_HPP

#include <boost/assert.hpp>
#include <boost/context/all.hpp>

#include <boost/config.hpp>
#include <boost/context/detail/config.hpp>

#include <iostream>

#include "TaskImpl.hpp"

namespace as {

class CoroutineTask
{
public:
	virtual TaskStatus Invoke() = 0;
	virtual void Yield() = 0;
};

namespace detail {

// static thread_local std::vector< CoroutineTask * > this_task_stack{};

struct coro_registry
{
	std::vector< CoroutineTask * > task_stack;

	~coro_registry()
	{
		std::cout << "DESTROYING THE MANNER!\n";
	}

	static coro_registry& this_stack() {
		static thread_local coro_registry instance{};
		return instance;
	}

	void push(CoroutineTask *c)
	{
		task_stack.insert( std::begin(task_stack), c );
	}

	void pop()
	{
		task_stack.erase( std::begin(task_stack) );
	}

	CoroutineTask *top()
	{
		return task_stack[0];
	}

	size_t size() const
	{
		return task_stack.size();
	}
};

template< std::size_t Max, std::size_t Default, std::size_t Min >
class simple_stack_allocator
{
public:
	static std::size_t maximum_stacksize()
	{ return Max; }

	static std::size_t default_stacksize()
	{ return Default; }

	static std::size_t minimum_stacksize()
	{ return Min; }

	void *allocate( std::size_t size ) const
	{
		BOOST_ASSERT( minimum_stacksize() <= size );
		BOOST_ASSERT( maximum_stacksize() >= size );

		void *limit = std::calloc( size, sizeof( char) );
		if ( !limit )
			throw std::bad_alloc();

		return static_cast< char * >( limit ) + size;
	}

	void deallocate( void * vp, std::size_t size ) const
	{
		BOOST_ASSERT( vp );
		BOOST_ASSERT( minimum_stacksize() <= size );
		BOOST_ASSERT( maximum_stacksize() >= size );

		void *limit = static_cast< char * >( vp ) - size;
		std::free( limit );
	}
};

} //namespace as::detail

namespace {
constexpr std::size_t MAX_STACK_SIZE = 1024*1024*8; // 8MB

constexpr std::size_t MIN_STACK_SIZE = 64*1024; // 64kB

constexpr std::size_t DEFAULT_STACK_SIZE = MIN_STACK_SIZE;
} // anonymous namespace

#if 0
namespace v1 {

namespace ctx = boost::ctx;

struct BoostContext
{
	typedef void (*EntryPointFunc)(intptr_t);

	ctx::fcontext_t ctxt;
	ctx::fcontext_t prev_ctxt;
	EntryPointFunc entry_point;
	intptr_t entry_arg;

	BoostContext( void *stack,
	              std::size_t stack_size,
	              EntryPointFunc func,
	              intptr_t arg )
		: ctxt()
		, prev_ctxt
		, entry_point(func)
		, entry_arg(arg)
	{
		ctxt.fc_stack.base = stack;
		ctxt.fc_stack.limit = static_cast< char * >(stack) - stack_size;

		Init();
	}

	void Init()
	{
		ctx::make_fcontext( &ctxt, entry_point );
	}

	void Invoke()
	{
		Jump( &prev_ctxt, &ctxt, entry_arg );
	}

	void Yield()
	{
		Jump( &ctxt, &prev_ctxt, entry_arg );
	}

	void Exit()
	{
		static ctx::fcontext_t noreturn_ctx;

		Init();

		Jump( &noreturn_ctx, &prev_ctxt, entry_arg );
	}

	void Jump( ctx::fcontext_t *orig_fctx,
	           ctx::fcontext_t *next_fctx,
	           intptr_t arg = 0 )
	{
		ctx::jump_fcontext( orig_fctx, next_fctx, arg );
	}
};
}
#endif // 0

namespace v2 {

namespace ctx = boost::context;

struct BoostContext
{
	typedef void (*EntryPointFunc)(intptr_t);

	ctx::fcontext_t ctxt;
	ctx::fcontext_t prev_ctxt;
	EntryPointFunc entry_point;
	intptr_t entry_arg;

	BoostContext( EntryPointFunc func, intptr_t arg )
		: ctxt()
		, prev_ctxt()
		, entry_point(func)
		, entry_arg(arg)
	{}

	void Init(void *stack, std::size_t stack_size)
	{
		ctxt = ctx::make_fcontext( stack, stack_size, entry_point );
	}

	void Invoke()
	{
		Jump( &prev_ctxt, &ctxt, entry_arg );
	}

	void Yield()
	{
		Jump( &ctxt, &prev_ctxt, entry_arg );
	}

	void Exit()
	{
		static ctx::fcontext_t noreturn_ctx;

		//Init();

		Jump( &noreturn_ctx, &prev_ctxt, entry_arg );
	}

	void Jump( ctx::fcontext_t *orig_fctx,
	           ctx::fcontext_t *next_fctx,
	           intptr_t arg = 0 )
	{
		ctx::jump_fcontext( orig_fctx, *next_fctx, arg );
	}
};

} // namespace v2

template<class TaskFunc>
class CoroutineTaskPriv
{
	typedef detail::simple_stack_allocator<
		MAX_STACK_SIZE,
		DEFAULT_STACK_SIZE,
		MIN_STACK_SIZE
	                                      > stack_allocator;

	stack_allocator alloc;
	std::size_t stack_size;
	void *stack;
	v2::BoostContext bctxt;
	invocation<TaskFunc> taskfunc;
	bool running;

private:
	void deinitialize_context()
	{
		// assert( stack );
		if ( stack ) {
			alloc.deallocate( stack, stack_allocator::default_stacksize() );
		}
	}

	static void entry_point( intptr_t p )
	{
		auto self = reinterpret_cast< CoroutineTaskPriv * >(p);

		self->running = true;

		self->on_entry();

		self->running = false;

		self->bctxt.Init( self->stack, self->stack_size );
		self->bctxt.Exit();
	}

	void on_entry()
	{
		taskfunc();
	}

public:
	CoroutineTaskPriv()
		: alloc()
		, stack_size( stack_allocator::default_stacksize() )
		, stack( alloc.allocate( stack_size ) )
		, bctxt( &CoroutineTaskPriv::entry_point,
		         reinterpret_cast<intptr_t>(this) )
		, running(false)
	{
		bctxt.Init( stack, stack_size );
	}

	CoroutineTaskPriv(typename invocation<TaskFunc>::func_type func)
		: alloc()
		, stack_size( stack_allocator::default_stacksize() )
		, stack( alloc.allocate( stack_size ) )
		, bctxt( &CoroutineTaskPriv::entry_point,
		         reinterpret_cast<intptr_t>(this) )
		, taskfunc( std::move(func) )
		, running(false)
	{
		bctxt.Init( stack, stack_size );
	}

	~CoroutineTaskPriv()
	{
		if ( !running )
			deinitialize_context();
	}

	CoroutineTaskPriv(CoroutineTaskPriv&& other)
		: alloc( std::move(other.alloc) )
		, stack_size( other.stack_size )
		, stack( other.stack )
		, bctxt( other.bctxt )
		, taskfunc( std::move(other.taskfunc) )
		, running( other.running )
	{
		if ( this == &other )
			return;

		other.stack = nullptr;
		other.running = false;
	}

public:
	TaskStatus Invoke()
	{
		bctxt.Invoke();

		return running ? TaskStatus::Repeat : TaskStatus::Finished;
	}

	void Yield()
	{
		bctxt.Yield();
	}

	void Cancel()
	{}
};

template<class TaskFunc>
class CoroutineTaskImpl
	: public CoroutineTask
{
	std::unique_ptr< CoroutineTaskPriv<TaskFunc> > priv;

public:
	CoroutineTaskImpl()
		: priv( new CoroutineTaskPriv<TaskFunc>() )
	{}

	CoroutineTaskImpl(typename invocation<TaskFunc>::func_type func)
		: priv( new CoroutineTaskPriv<TaskFunc>(std::move(func)) )
	{}

public:
	TaskStatus Invoke()
	{
		assert( priv );

		//detail::this_task_stack.insert( std::begin(detail::this_task_stack), this );

		detail::coro_registry::this_stack().push( this );

		auto r = priv->Invoke();

		detail::coro_registry::this_stack().pop();

		//detail::this_task_stack.erase( std::begin(detail::this_task_stack) );

		return r;
	}

	void Yield()
	{
		assert( priv );

		priv->Yield();
	}

	void Cancel()
	{}
};

namespace this_task {

inline void yield()
{
	// if ( detail::this_task_stack.size() == 0 )
	// 	return;

	if ( detail::coro_registry::this_stack().size() == 0 )
		return;

	detail::coro_registry::this_stack().top()->Yield();

	//detail::this_task_stack[0]->Yield();
}

} // namespace as::ThisTask

} // namespace as

#endif // AS_COROUTINE_TASK_IMPL_HPP
