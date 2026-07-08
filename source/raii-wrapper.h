#ifndef RAII_WRAPPER_H
#define RAII_WRAPPER_H

#include <memory>

#define MAKE_RAII_POINTER(NAME, TYPE, DELETER) \
using NAME##Base = std::unique_ptr<TYPE, decltype([](TYPE* const pointer){ DELETER(pointer); })>; \
 \
class NAME : public NAME##Base \
{ \
public: \
	using NAME##Base::NAME##Base; \
 \
	operator TYPE*() { return get(); } \
	operator const TYPE*() const { return get(); } \
}

#endif // RAII_WRAPPER_H
