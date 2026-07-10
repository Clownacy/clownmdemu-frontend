#ifndef RAII_WRAPPER_H
#define RAII_WRAPPER_H

#include <memory>
#include <type_traits>
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

	template<typename T>
	struct FunctionMetadata;

	template<typename ReturnType, typename ParameterType>
	struct FunctionMetadata<ReturnType(*)(ParameterType)>
	{
		using return_type = ReturnType;
		using parameter_type = ParameterType;
	};

	template<auto Delete>
	using PointerBase = std::unique_ptr<std::remove_pointer_t<typename FunctionMetadata<decltype(Delete)>::parameter_type>, Deleter<Delete>>;

	template<auto Delete>
	class Pointer : public PointerBase<Delete>
	{
	public:
		using PointerBase<Delete>::PointerBase;

		operator auto() { return this->get(); }
		operator auto() const { return this->get(); }
	};
}

#endif // RAII_WRAPPER_H
