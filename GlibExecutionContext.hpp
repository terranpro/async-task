#ifndef AS_GLIB_EXECUTION_CONTEXT_HPP
#define AS_GLIB_EXECUTION_CONTEXT_HPP

#include "ExecutionContext.hpp"

#include <map>
#include <vector>

#include <cassert>

#include <glib.h>

namespace as {

struct GlibThread
{
	GThread *context_thread;
	GMainLoop *loop;

	GlibThread(GMainLoop *l)
		: context_thread()
		, loop( l ) // assume control of pointer, do not ref here
	{
		context_thread = g_thread_new( "GlibExecutionContext",
		                               &GlibThread::ThreadEntryPoint,
		                               g_main_loop_ref(loop) );
	}

	GlibThread(GThread *t)
		: context_thread(t)
		, loop()
	{}

	~GlibThread()
	{
		if ( loop )
			Shutdown();
	}

	operator GThread *()
	{
		return context_thread;
	}

private:
	GlibThread(GlibThread const&) = delete;
	GlibThread& operator=(GlibThread const&) = delete;

private:
	static gpointer ThreadEntryPoint(gpointer user_data)
	{
		GMainLoop *loop = static_cast<GMainLoop *>( user_data );

		assert( loop );

		g_main_loop_run( loop );

		g_main_loop_unref( loop );

		return NULL;
	}

	void Shutdown()
	{
		assert( loop );

		GMainContext *context =
			g_main_context_ref( g_main_loop_get_context( loop ) );

		if ( g_main_context_default() == context ) {
			// TODO: assert feasibility
			return g_main_context_unref( context );
		}

		auto quit_src = g_idle_source_new();

		g_source_set_priority( quit_src, G_PRIORITY_LOW );

		g_source_set_callback( quit_src,
		                       [](gpointer user_data) {
			                       auto self = static_cast<GMainLoop *>(user_data);

			                       auto ctxt = g_main_loop_get_context(self);

			                       while( g_main_context_iteration( ctxt, FALSE ) ) {
				                       // dispatch until no more events are pending
			                       }

			                       g_main_loop_quit( self );
			                       g_main_loop_unref( self );

			                       return FALSE;
		                       },
		                       g_main_loop_ref(loop),
		                       NULL );

		g_source_attach( quit_src, context );

		g_source_unref( quit_src );

		g_main_context_unref( context );

		g_main_loop_unref( loop );
		loop = NULL;

		g_thread_unref( context_thread );
		context_thread = NULL;
	}
};

struct GlibExecutionContextImpl
{
	typedef Task TaskPair;

	GMainContext *context;
	GlibThread gthread;

	GlibExecutionContextImpl()
		: context( g_main_context_new() )
		, gthread( g_main_loop_new( context, FALSE ) )
	{}

	GlibExecutionContextImpl(GMainContext *ctxt)
		: context( g_main_context_ref(ctxt) )
		, gthread( g_thread_self() )
	{}

	~GlibExecutionContextImpl()
	{
		if ( context )
			g_main_context_unref( context );
	}

	void AddTask(Task task)
	{
		CreateIdle( task );
	}

	void AddTimedTask(Task task, std::chrono::milliseconds const& time_ms)
	{
		CreateTimer( task, time_ms.count() );
	}

	void Iteration()
	{
		g_main_context_iteration( context, FALSE );
	}

	bool IsCurrent()
	{
		return g_thread_self() == gthread;
	}

private:
	void CreateIdle(Task task)
	{
		auto idler_source = g_idle_source_new();
		AddTaskPair( idler_source, CreateTaskPair( std::move(task) ) );

		g_source_unref( idler_source );
	}

	void CreateTimer(Task task, unsigned int time_ms)
	{
		auto timer_source = g_timeout_source_new( time_ms );
		AddTaskPair( timer_source, CreateTaskPair( std::move(task) ) );

		g_source_unref( timer_source );
	}

	std::unique_ptr<TaskPair>
	CreateTaskPair(Task task)
	{
		return std::unique_ptr<TaskPair>{ new TaskPair{ std::move(task) } };
	}

	void AddTaskPair(GSource *source, std::unique_ptr<TaskPair> tpair)
	{
		g_source_set_callback( source,
		                       TaskCb,
		                       tpair.release(),
		                       TaskPairDestroyNotify );

		g_source_attach( source, context );
	}

	static gboolean TaskCb(gpointer user_data)
	{
		// Don't use unique_ptr here - TaskPairDestroyNotify will cleanup
		// when the callback is no longer in use
		TaskPair *tpair = static_cast<TaskPair *>(user_data);

		return DispatchTask( *tpair );
	}

	static void TaskPairDestroyNotify(void *user_data)
	{
		assert( user_data );

		std::unique_ptr<TaskPair> tpair{ static_cast<TaskPair *>(user_data) };
	}

	static gboolean DispatchTask(Task& task)
	{
		task.Invoke();

		// TODO: enable finished/again/continue detection
		if ( task.IsFinished() )
			return false;

		return true;
	}
};

// Public Interface Definition

class GlibExecutionContext
	: public ExecutionContext
{
	std::unique_ptr<GlibExecutionContextImpl> impl;

	GlibExecutionContext(void *context);

public:
	static GlibExecutionContext& GetDefault();

public:
	GlibExecutionContext();
	~GlibExecutionContext();

	void AddTask(Task task);
	void AddTimedTask(Task task, std::chrono::milliseconds time_ms);

	void Iteration();
	bool IsCurrent();
};

//private:
GlibExecutionContext::GlibExecutionContext(void *context)
	: impl( new GlibExecutionContextImpl( static_cast<GMainContext *>(context) ) )
{}

//public:
GlibExecutionContext::GlibExecutionContext()
	: impl( new GlibExecutionContextImpl )
{}

GlibExecutionContext::~GlibExecutionContext() = default;

// static - get default main context
GlibExecutionContext& GlibExecutionContext::GetDefault()
{
	// Default Main Context initialization
	static GlibExecutionContext main_context{ g_main_context_default() };

	return main_context;
}

void GlibExecutionContext::AddTimedTask(Task task, std::chrono::milliseconds time_ms)
{
	impl->AddTimedTask( task, time_ms );
}

void GlibExecutionContext::AddTask(Task task)
{
	impl->AddTask( task );
}

void GlibExecutionContext::Iteration()
{
	impl->Iteration();
}

bool GlibExecutionContext::IsCurrent()
{
	return impl->IsCurrent();
}

} // namespace as

#endif // AS_GLIB_EXECUTION_CONTEXT_HPP
