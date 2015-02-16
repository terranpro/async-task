#ifndef AS_TASK_EXECUTOR_HPP
#define AS_TASK_EXECUTOR_HPP

struct TaskExecutorBase
{
	virtual ~TaskExecutorBase() {}

	virtual void Run() = 0;
	virtual bool IsFinished() const = 0;
};

#endif // AS_TASK_EXECUTOR_HPP
