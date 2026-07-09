#ifndef RAII_WRAPPER_H
#define RAII_WRAPPER_H

#include <memory>
#include <utility>

namespace RAII
{
	template<auto Delete>
	struct Deleter
	{
		template<typename T>
		auto operator()(T &&pointer)
		{
			return Delete(std::forward<T>(pointer));
		}
	};

	template<typename Type, auto Delete>
	class Pointer : public std::unique_ptr<Type, Deleter<Delete>>
	{
	public:
		using std::unique_ptr<Type, Deleter<Delete>>::unique_ptr;

		operator Type*() { return this->get(); }
		operator const Type*() const { return this->get(); }
	};
}

#endif // RAII_WRAPPER_H
