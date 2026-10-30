#ifndef SPAN_H
#define SPAN_H

// The span library that we use is bugged, and fails to include this header.
// As a result, `std::terminate` is used without being declared.
#include <exception>

#include <tcb/span.hpp>

#endif /* SPAN_H */
