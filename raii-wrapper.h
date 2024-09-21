#ifndef RAII_WRAPPER_H
#define RAII_WRAPPER_H

#include <memory>

#define MAKE_RAII_POINTER(NAME, TYPE, DELETER) \
struct NAME##Deleter \
{ \
	void operator()(TYPE* const pointer) { DELETER(pointer); } \
}; \
 \
using NAME = std::unique_ptr<TYPE, NAME##Deleter>

#endif // RAII_WRAPPER_H
