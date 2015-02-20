#ifndef AS_COROUTINE_TASK_HPP
#define AS_COROUTINE_TASK_HPP

#include <boost/assert.hpp>
#include <boost/context/all.hpp>

#include <boost/config.hpp>
#include <boost/context/detail/config.hpp>

#include "TaskContext.hpp"

namespace as {

namespace detail {

static thread_local std::vector< TaskContext * > this_task_stack{};

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

		void *limit = static_cast< char * >( vp) - size;
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

	ctx::fcontext_t *ctxt;
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
		Jump( &prev_ctxt, ctxt, entry_arg );
	}

	void Yield()
	{
		Jump( ctxt, &prev_ctxt, entry_arg );
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
		ctx::jump_fcontext( orig_fctx, next_fctx, arg );
	}
};

} // namespace v2


class CoroutineTaskContext
	: public TaskContext
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
	std::shared_ptr<TaskFunctionBase> taskfunc;

private:
	void deinitialize_context()
	{
		assert( stack );

		alloc.deallocate( stack, stack_allocator::default_stacksize() );
	}

	static void entry_point( intptr_t p )
	{
		auto self = reinterpret_cast< CoroutineTaskContext * >(p);

		self->on_entry();

		self->bctxt.Init( self->stack, self->stack_size );
		self->bctxt.Exit();
	}

	void on_entry()
	{
		if (taskfunc)
			taskfunc->Run();
	}

public:
	CoroutineTaskContext()
		: alloc()
		, stack_size( stack_allocator::default_stacksize() )
		, stack( alloc.allocate( stack_size ) )
		, bctxt( &CoroutineTaskContext::entry_point,
		         reinterpret_cast<intptr_t>(this) )
		, taskfunc()
	{
		bctxt.Init( stack, stack_size );
	}

	CoroutineTaskContext(std::shared_ptr<TaskFunctionBase> te)
		: alloc()
		, stack_size( stack_allocator::default_stacksize() )
		, stack( alloc.allocate( stack_size ) )
		, bctxt( &CoroutineTaskContext::entry_point,
		         reinterpret_cast<intptr_t>(this) )
		, taskfunc( te )
	{
		bctxt.Init( stack, stack_size );
	}

	~CoroutineTaskContext()
	{
		deinitialize_context();
	}

public:
	void Invoke()
	{
		detail::this_task_stack.insert( std::begin(detail::this_task_stack), this );

		bctxt.Invoke();

		detail::this_task_stack.erase( std::begin(detail::this_task_stack) );
	}

	void Yield()
	{
		bctxt.Yield();
	}
};

namespace ThisTask {

inline void Yield()
{
	if ( detail::this_task_stack.size() == 0 )
		return;

	detail::this_task_stack[0]->Yield();
}

} // namespace as::ThisTask

} // namespace as

#endif // AS_COROUTINE_TASK_HPP
