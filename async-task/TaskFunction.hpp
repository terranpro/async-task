//
//  bind.hpp - binds function objects to arguments
//
//  Copyright (c) 2015 Brian Fransioli
//
//  Distributed under the Boost Software License, Version 1.0.
//  See accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt
//

#ifndef AS_TASK_FUNCTION_HPP
#define AS_TASK_FUNCTION_HPP

struct TaskFunctionBase
{
	virtual ~TaskFunctionBase() {}

	virtual void Run() = 0;
	virtual bool IsFinished() const = 0;
};

#endif // AS_TASK_FUNCTION_HPP
