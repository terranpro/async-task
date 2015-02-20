#ifndef AS_TASK_FUNCTION_HPP
#define AS_TASK_FUNCTION_HPP

struct TaskFunctionBase
{
	virtual ~TaskFunctionBase() {}

	virtual void Run() = 0;
	virtual bool IsFinished() const = 0;
};

#endif // AS_TASK_FUNCTION_HPP
