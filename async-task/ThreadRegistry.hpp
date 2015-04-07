#ifndef AS_THREAD_REGISTRY
#define AS_THREAD_REGISTRY

#include <map>
#include <typeinfo>

namespace as {

template<class Executor, class Context>
struct Registry
{
	static thread_local std::map< Executor *, Context * > registry;
	Executor *ex;

	Registry(Executor *ex, Context *ctxt)
		: ex(ex)
	{
		registry[ex] = ctxt;
	}

	~Registry()
	{
		registry.erase( ex );
	}

	static Context *Current(Executor *ex)
	{
		auto it = registry.find( ex );
		if ( it != registry.end() )
			return *it;

		return nullptr;
	}
};

} // namespace as

#endif // AS_THREAD_REGISTRY
