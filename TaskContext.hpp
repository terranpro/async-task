#ifndef AS_TASK_CONTEXT_HPP
#define AS_TASK_CONTEXT_HPP

namespace as {

struct TaskExecutionConcept
{
	virtual ~TaskExecutionConcept() {}

	virtual void Run() = 0;
	virtual bool IsFinished() const = 0;
};

class TaskContext
{
public:
	virtual ~TaskContext() {}

	virtual void Invoke() = 0;
	virtual void Yield() = 0;
};

} // namespace as

#endif // AS_TASK_CONTEXT_HPP
