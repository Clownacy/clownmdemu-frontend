#ifndef INPUT_H
#define INPUT_H

#include <array>
#include <list>
#include <set>
#include <string>

#include <SDL3/SDL.h>
#include <fmt/core.h>

#include "../common/core/source/clownmdemu.h"

enum class InputBinding
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

class Input
{
public:
	const std::string name;

	Input(std::string name)
		: name(std::move(name))
	{}

	unsigned int GetButton(InputBinding button) const;
	virtual unsigned int GetButtonInternal(InputBinding button) const = 0;
};

class KeyboardInput : public Input
{
private:
	using BindingsBase = std::array<std::set<SDL_Scancode>, static_cast<std::size_t>(InputBinding::TOTAL)>;

	class Bindings : public BindingsBase
	{
	public:
		using BindingsBase::operator[];

		auto& operator[](const InputBinding index) const
		{
			return operator[](static_cast<std::size_t>(index));
		}

		auto& operator[](const InputBinding index)
		{
			return operator[](static_cast<std::size_t>(index));
		}
	};

public:
	Bindings bindings;

	using Input::Input;

	virtual unsigned int GetButtonInternal(InputBinding button) const override;
};

class ControllerInput : public Input
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

	ControllerInput(const SDL_JoystickID joystick_instance_id)
		: joystick_instance_id(joystick_instance_id)
		, Input(fmt::format("Controller {}: {}", ++controller_number, SDL_GetJoystickNameForID(joystick_instance_id)))
	{}

	virtual unsigned int GetButtonInternal(InputBinding button) const override;
};

extern KeyboardInput keyboard_input;
extern std::list<ControllerInput> controller_input_list;

extern std::array<const Input*, 8> bound_inputs;

extern ControllerLayout controller_layout;

#endif // INPUT_H
