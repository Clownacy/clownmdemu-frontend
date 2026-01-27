#ifndef SDL_WRAPPER_H
#define SDL_WRAPPER_H

#include <filesystem>
#include <string>

#include <SDL3/SDL.h>

#include "raii-wrapper.h"

namespace SDL
{
	struct FreeFunctor
	{
		void operator()(void* const pointer) { return SDL_free(pointer); }
	};

	template<typename T>
	using Pointer = std::unique_ptr<T, FreeFunctor>;

	MAKE_RAII_POINTER(Window,       SDL_Window,       SDL_DestroyWindow     );
	MAKE_RAII_POINTER(Renderer,     SDL_Renderer,     SDL_DestroyRenderer   );
	MAKE_RAII_POINTER(Texture,      SDL_Texture,      SDL_DestroyTexture    );
	MAKE_RAII_POINTER(IOStreamBase, SDL_IOStream,     SDL_CloseIO           );
	MAKE_RAII_POINTER(AudioStream,  SDL_AudioStream,  SDL_DestroyAudioStream);
	MAKE_RAII_POINTER(SharedObject, SDL_SharedObject, SDL_UnloadObject      );

	template<typename T>
	auto MakePointer(T* const pointer)
	{
		return Pointer<T>(pointer);
	}

	template<auto Function, typename... Args>
	auto PathFunction(const char* const path, Args &&...args)
	{
		return Function(path, std::forward<Args>(args)...);
	}

	template<auto Function, typename... Args>
	auto PathFunction(const std::string &path, Args &&...args)
	{
		return Function(path.c_str(), std::forward<Args>(args)...);
	}

	template<auto Function, typename... Args>
	auto PathFunction(const std::filesystem::path &path, Args &&...args)
	{
		return Function(reinterpret_cast<const char*>(path.u8string().c_str()), std::forward<Args>(args)...);
	}

	template<typename T>
	bool RemovePath(const T &path) { return PathFunction<SDL_RemovePath>(path); }

	template<typename T>
	bool GetPathInfo(const T &path, SDL_PathInfo* const info) { return PathFunction<SDL_GetPathInfo>(path, info); }

	class IOStream : public IOStreamBase
	{
	public:
		using IOStreamBase::IOStreamBase;

		template<typename T>
		IOStream(const T &path, const char* const mode)
			: IOStreamBase(PathFunction<SDL_IOFromFile>(path, mode))
		{}

		IOStream(void* const mem, const std::size_t size)
			: IOStreamBase(SDL_IOFromMem(mem, size))
		{}

		IOStream(const void* const mem, const std::size_t size)
			: IOStreamBase(SDL_IOFromConstMem(mem, size))
		{}
	};
}

#endif /* SDL_WRAPPER_H */
