#ifndef SDL_WRAPPER_H
#define SDL_WRAPPER_H

#include <filesystem>
#include <functional>
#include <string>
#include <string_view>

#include <SDL3/SDL.h>
#include <tcb/span.hpp>

#include "../libraries/function2/include/function2/function2.hpp"

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
	MAKE_RAII_POINTER(Surface,      SDL_Surface,      SDL_DestroySurface    );
	MAKE_RAII_POINTER(IOStreamBase, SDL_IOStream,     SDL_CloseIO           );
	MAKE_RAII_POINTER(AudioStream,  SDL_AudioStream,  SDL_DestroyAudioStream);
	MAKE_RAII_POINTER(SharedObject, SDL_SharedObject, SDL_UnloadObject      );

	inline std::filesystem::path U8Path(const std::string_view &string)
	{
		return std::u8string_view(reinterpret_cast<const char8_t*>(std::data(string)), std::size(string));
	}

	inline auto PathToStringView(const std::filesystem::path &path, const auto &callback)
	{
		const auto &string = path.u8string();
		return callback(std::string_view(reinterpret_cast<const char*>(std::data(string)), std::size(string)));
	}

	inline auto PathToCString(const std::filesystem::path &path, const auto &callback)
	{
		const auto &string = path.u8string();
		return callback(reinterpret_cast<const char*>(std::data(string)));
	}

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
		return PathToCString(path,
			[&](const char* const string)
			{
				return Function(string, std::forward<Args>(args)...);
			}
		);
	}

	template<typename T>
	bool RemovePath(const T &path) { return PathFunction<SDL_RemovePath>(path); }

	template<typename T>
	bool GetPathInfo(const T &path, SDL_PathInfo* const info) { return PathFunction<SDL_GetPathInfo>(path, info); }

	template<typename... Ts>
	std::filesystem::path GetPrefPath(Ts &&...args)
	{
		const auto path_cstr = SDL::MakePointer(SDL_GetPrefPath(std::forward<Ts>(args)...));

		if (path_cstr == nullptr)
			return "";

		return U8Path(path_cstr.get());
	}

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

		IOStream()
			: IOStreamBase(SDL_IOFromDynamicMem())
		{}
	};

	inline bool RunOnMainThread(const SDL_MainThreadCallback main_thread_callback, const void* const user_data = nullptr, const bool wait_complete = true)
	{
		return SDL_RunOnMainThread(main_thread_callback, const_cast<void*>(user_data), wait_complete);
	}

	using MainThreadCallback = fu2::unique_function<void()>;
	inline bool RunOnMainThread(MainThreadCallback main_thread_callback)
	{
		return RunOnMainThread(
			[](void* const user_data)
			{
				auto &callback = *static_cast<MainThreadCallback*>(user_data);

				callback();
			},
			&main_thread_callback, true
		);
	}

	inline void ShowFileDialogWithProperties(const SDL_FileDialogType type, const SDL_DialogFileCallback callback, void *const userdata, const SDL_PropertiesID props)
	{
		SDL_ShowFileDialogWithProperties(type, callback, userdata, props);
	}

	using DialogFileCallback = fu2::unique_function<void(const char* const *file_list, int filter)>;
	inline void ShowFileDialogWithProperties(const SDL_FileDialogType type, DialogFileCallback callback, const SDL_PropertiesID props)
	{
		// Create copy of the callback so that it outlives its caller.
		// `ShowFileDialogWithProperties` may run the callback on a different thread.
		const auto callback_copy = new DialogFileCallback(std::move(callback));

		ShowFileDialogWithProperties(
			type,
			[](void* const user_data, const char* const* const file_list, const int filter)
			{
				// Automatically handles freeing the callback copy.
				const auto callback = static_cast<DialogFileCallback*>(user_data);

				(*callback)(file_list, filter);

				delete callback;
			},
			callback_copy,
			props
		);
	}

	using ClipboardDataCallback = fu2::unique_function<const void*(const char *mime_type, size_t *size)>;
	inline bool SetClipboardData(ClipboardDataCallback callback, const tcb::span<const char* const> &mime_types)
	{
		const auto callback_copy = new ClipboardDataCallback(std::move(callback));

		const auto &CallbackWrapper = [](void *user_data, const char *mime_type, std::size_t *size) -> const void*
		{
			auto &callback = *static_cast<ClipboardDataCallback*>(user_data);

			return callback(mime_type, size);
		};

		const auto &CleanupWrapper = [](void *user_data)
		{
			delete static_cast<ClipboardDataCallback*>(user_data);
		};

		// TODO: SDL v3.4.4 has dumb constness. Once updated, scrap the nasty const cast.
		return SDL_SetClipboardData(CallbackWrapper, CleanupWrapper, callback_copy, const_cast<const char**>(std::data(mime_types)), std::size(mime_types));
	}

	template<typename Callback>
	inline auto SetRenderTarget(SDL_Renderer* const renderer, SDL_Texture* const texture, const Callback &callback)
	{
		class RenderTarget
		{
		private:
			SDL_Renderer* const renderer;
			SDL_Texture* const previous_render_target;

		public:
			RenderTarget(SDL_Renderer* const renderer, SDL_Texture* const texture)
				: renderer(renderer)
				, previous_render_target(SDL_GetRenderTarget(renderer))
			{
				SDL_SetRenderTarget(renderer, texture);
			}

			~RenderTarget()
			{
				SDL_SetRenderTarget(renderer, previous_render_target);
			}
		};

		RenderTarget render_target(renderer, texture);

		return callback();
	}
}

#endif /* SDL_WRAPPER_H */
