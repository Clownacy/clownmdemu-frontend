#ifndef INPUT_H
#define INPUT_H

#include <array>
#include <list>
#include <set>
#include <string>

#include <SDL3/SDL.h>
#include <fmt/core.h>

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

	enum class ControllerLayout
	{
		FOUR_BUTTON,
		SIX_BUTTON,
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
	};

	class Keyboard : public Device
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

	public:
		Bindings bindings;

		using Device::Device;

		virtual unsigned int GetButtonInternal(Binding button) const override;
	};

	class Controller : public Device
	{
	public:
		static inline unsigned int controller_number;

		const SDL_JoystickID joystick_instance_id;
		Sint16 left_stick_x = 0;
		Sint16 left_stick_y = 0;
		std::array<bool, 4> left_stick = {false};
		std::array<bool, 4> dpad = {false};
		bool left_trigger = false;
		bool right_trigger = false;

		Controller(const SDL_JoystickID joystick_instance_id)
			: joystick_instance_id(joystick_instance_id)
			, Device(fmt::format("Controller {}: {}", ++controller_number, SDL_GetJoystickNameForID(joystick_instance_id)))
		{}

		virtual unsigned int GetButtonInternal(Binding button) const override;
	};

	extern Keyboard keyboard;
	extern std::list<Controller> controllers;

	extern std::array<const Device*, 8> bound_devices;

	extern ControllerLayout controller_layout;
}

#endif // INPUT_H
