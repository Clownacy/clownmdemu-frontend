#ifndef RAII_WRAPPER_H
#define RAII_WRAPPER_H

#include <memory>

#define MAKE_RAII_DELETER(NAME, TYPE, DELETER) \
struct NAME##Deleter \
{ \
	void operator()(TYPE* const pointer) { DELETER(pointer); } \
}

#define MAKE_RAII_POINTER(NAME, TYPE, DELETER) \
MAKE_RAII_DELETER(NAME, TYPE, DELETER); \
 \
class NAME : public std::unique_ptr<TYPE, NAME##Deleter> \
{ \
public: \
	using std::unique_ptr<TYPE, NAME##Deleter>::unique_ptr; \
 \
	operator TYPE*() { return get(); } \
	operator const TYPE*() const { return get(); } \
}

#endif // RAII_WRAPPER_H
