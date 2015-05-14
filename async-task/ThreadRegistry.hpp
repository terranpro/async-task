#ifndef AS_THREAD_REGISTRY
#define AS_THREAD_REGISTRY

#include <map>
#include <typeinfo>

namespace as {

template<class Executor, class Context>
struct Registry
{
	static __thread std::map< Executor const *, Context * > *registry;
	Executor *ex;

	Registry(Executor *ex, Context *ctxt)
		: ex(ex)
	{
		if ( !registry )
			registry = new std::map< Executor const *, Context * >();

		this->registry->insert( std::make_pair(ex, ctxt) );
	}

	~Registry()
	{
		registry->erase( ex );
		if ( registry->empty() ) {
			delete registry;
			registry = nullptr;
		}
	}

	static Context *Current(Executor const *ex)
	{
		if ( !registry )
			return nullptr;

		auto it = registry->find( ex );
		if ( it != registry->end() )
			return it->second;

		return nullptr;
	}
};

template<class Executor, class Context>
//thread_local std::map< Executor *, Context * > Registry<Executor, Context>::registry{};
__thread std::map< Executor const *, Context * > *Registry<Executor, Context>::registry;

} // namespace as

#endif // AS_THREAD_REGISTRY
