#ifndef INPUT_H
#define INPUT_H

#include <array>
#include <list>
#include <set>
#include <string>

#include <SDL3/SDL.h>

#include "../common/core/source/clownmdemu.h"

namespace Input
{
	enum class Binding
	{
		NONE,
		CONTROLLER_UP,
		CONTROLLER_DOWN,
		CONTROLLER_LEFT,
		CONTROLLER_RIGHT,
		CONTROLLER_A,
		CONTROLLER_B,
		CONTROLLER_C,
		CONTROLLER_X,
		CONTROLLER_Y,
		CONTROLLER_Z,
		CONTROLLER_START,
		CONTROLLER_MODE,
		PAUSE,
		RESET,
		FAST_FORWARD,
		REWIND,
		QUICK_SAVE_STATE,
		QUICK_LOAD_STATE,
		TOGGLE_FULLSCREEN,
		TOTAL,
	};

	class Device
	{
	public:
		const std::string name;

		Device(std::string name)
			: name(std::move(name))
		{}

		unsigned int GetButton(Binding button) const;
		virtual unsigned int GetButtonInternal(Binding button) const = 0;

		void AutoBind() const;
	};

	class Keyboard final : public Device
	{
	private:
		using BindingsBase = std::array<std::set<SDL_Scancode>, static_cast<std::size_t>(Binding::TOTAL)>;

		class Bindings : public BindingsBase
		{
		public:
			using BindingsBase::operator[];

			auto& operator[](const Binding index) const
			{
				return operator[](static_cast<std::size_t>(index));
			}

			auto& operator[](const Binding index)
			{
				return operator[](static_cast<std::size_t>(index));
			}
		};

		virtual unsigned int GetButtonInternal(Binding button) const override;

	public:
		Bindings bindings;

		using Device::Device;
	};

	class Controller final : public Device
	{
	private:
		static inline unsigned int controller_number;

		virtual unsigned int GetButtonInternal(Binding button) const override;

	public:
		enum class Layout
		{
			FOUR_BUTTON,
			SIX_BUTTON,
		};

		static inline Layout layout;

		const SDL_JoystickID joystick_instance_id;

		Controller(SDL_JoystickID joystick_instance_id);
	};

	extern Keyboard keyboard;
	extern std::list<Controller> controllers;

	extern std::array<const Device*, 8> bound_devices;
}

#endif // INPUT_H
