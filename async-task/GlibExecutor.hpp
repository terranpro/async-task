//
//  GlibExecutor.hpp - Glib based executor implementation
//
//  Copyright (c) 2015 Brian Fransioli
//
//  Distributed under the Boost Software License, Version 1.0.
//  See accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt
//

#ifndef AS_GLIB_EXECUTOR_HPP
#define AS_GLIB_EXECUTOR_HPP

#include "Executor.hpp"

#include <map>
#include <vector>

#include <cassert>

#include <glib.h>

namespace as {

class GlibThread
{
	GThread *context_thread;
	GMainLoop *loop;

public:
	GlibThread(GMainLoop *l)
		: context_thread()
		, loop( l ) // assume control of pointer, do not ref here
	{
		context_thread = g_thread_new( "GlibExecutor",
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

	GlibThread(GlibThread const&) = delete;
	GlibThread& operator=(GlibThread const&) = delete;

	operator GThread *() const
	{
		return context_thread;
	}

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

struct GlibExecutorImpl
{
	typedef Task TaskPair;

	GMainContext *context;
	GlibThread gthread;

	GlibExecutorImpl()
		: context( g_main_context_new() )
		, gthread( g_main_loop_new( context, FALSE ) )
	{}

	GlibExecutorImpl(GMainContext *ctxt)
		: context( g_main_context_ref(ctxt) )
		, gthread( g_thread_self() )
	{}

	~GlibExecutorImpl()
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

	bool IsCurrent() const
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

class GlibExecutor
	: public Executor
{
	std::unique_ptr<GlibExecutorImpl> impl;

	GlibExecutor(void *context)
		: impl( new GlibExecutorImpl( static_cast<GMainContext *>(context) ) )
	{}

public:
	static GlibExecutor& GetDefault()
	{
		static GlibExecutor main_context{ g_main_context_default() };

		return main_context;
	}

public:
	GlibExecutor()
		: impl( new GlibExecutorImpl )
	{}

	void Schedule(Task task)
	{
		impl->AddTask( task );
	}

	void ScheduleAfter(Task task, std::chrono::milliseconds time_ms)
	{
		impl->AddTimedTask( task, time_ms );
	}

	void Iteration()
	{
		impl->Iteration();
	}

	bool IsCurrent() const
	{
		return impl->IsCurrent();
	}
};

Executor& Executor::GetDefault()
{
	return GlibExecutor::GetDefault();
}

} // namespace as

#endif // AS_GLIB_EXECUTOR_HPP
