#ifndef AS_TASK_CONTEXT_HPP
#define AS_TASK_CONTEXT_HPP

namespace as {

class TaskContext
{
public:
	virtual ~TaskContext() {}

	virtual void Invoke() = 0;
	virtual void Yield() = 0;
};

} // namespace as

#endif // AS_TASK_CONTEXT_HPP
