#ifndef RAII_WRAPPER_H
#define RAII_WRAPPER_H

#include <memory>

#define MAKE_RAII_POINTER(NAME, TYPE, DELETER) \
struct NAME##Deleter \
{ \
	void operator()(TYPE* const pointer) { DELETER(pointer); } \
}; \
 \
class NAME : private std::unique_ptr<TYPE, NAME##Deleter> \
{ \
public: \
	using std::unique_ptr<TYPE, NAME##Deleter>::unique_ptr; \
	using std::unique_ptr<TYPE, NAME##Deleter>::release; \
	using std::unique_ptr<TYPE, NAME##Deleter>::operator bool; \
 \
	operator TYPE*() { return get(); } \
	operator const TYPE*() const { return get(); } \
}

#endif // RAII_WRAPPER_H
