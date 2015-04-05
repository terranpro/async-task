//
//  TaskFuture.hpp - Asynchronous Task Result Future
//
//  Copyright (c) 2015 Brian Fransioli
//
//  Distributed under the Boost Software License, Version 1.0.
//  See accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt
//

#ifndef AS_TASK_FUTURE_HPP
#define AS_TASK_FUTURE_HPP

#include "TaskControlBlock.hpp"

#include <memory>

namespace as {

template<class T>
class TaskFuture
{

public:
	TaskFuture() = default;

};

} // namespace as

#endif // AS_TASK_FUTURE_HPP
