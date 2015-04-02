//
//  Sync.hpp - Sync dispatch of functions via executors and locked objects
//
//  Copyright (c) 2015 Brian Fransioli
//
//  Distributed under the Boost Software License, Version 1.0.
//  See accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt
//

#ifndef AS_SYNC_HPP
#define AS_SYNC_HPP

#include "Executor.hpp"

#include <memory>

namespace as {

template<class Func, class... Args>
auto sync(Executor& context, Func&& func, Args&&... args)
	-> decltype( std::declval<Func>()(std::declval<Args>()...) )
{
	if ( context.IsCurrent() )
		return std::forward<Func>(func)( std::forward<Args>(args)... );


	auto timpl = make_task<TaskImplBase>( std::forward<Func>(func),
	                                      std::forward<Args>(args)... );

	context.Schedule( {timpl} );

	return timpl->GetControlBlock()->Get();
}

template<class Func, class... Args>
decltype( std::declval<Func>()(std::declval<Args>()...) )
sync(std::shared_ptr<Executor> context, Func&& func, Args&&... args)
{
	return sync( *context, std::forward<Func>(func), std::forward<Args>(args)... );
}

template<class Func, class... Args>
decltype( std::declval<Func>()(std::declval<Args>()...) )
sync(Func&& func, Args&&... args)
{
	return std::forward<Func>(func)( std::forward<Args>(args)... );
}

} // namespace as

#endif // AS_SYNC_HP
