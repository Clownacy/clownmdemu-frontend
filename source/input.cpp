#include "input.h"

#include <fmt/format.h>

Input::Keyboard Input::keyboard = {"Keyboard"};
std::list<Input::Controller> Input::controllers;

std::array<const Input::Device*, 8> Input::bound_devices;

void Input::Device::AutoBind() const
{
	// bind this controller to the first unbound input.
	for (auto &bound_input : bound_devices)
		if (bound_input == this)
			return;

	for (auto &bound_input : bound_devices)
	{
		if (bound_input == nullptr)
		{
			bound_input = this;
			break;
		}
	}
}

unsigned int Input::Keyboard::GetButton(const Binding button) const
{
	int total_keys;
	const bool* const keyboard_state = SDL_GetKeyboardState(&total_keys);

	if (keyboard_state == nullptr)
		return false;

	unsigned int total_presses = 0;

	for (const auto scancode : bindings[button])
		if (keyboard_state[scancode])
			++total_presses;

	return total_presses;
}

Input::Controller::Controller(const SDL_JoystickID joystick_instance_id)
	: Device(fmt::format("Controller {}: {}", ++controller_number, SDL_GetJoystickNameForID(joystick_instance_id)))
	, joystick_instance_id(joystick_instance_id)
{}

unsigned int Input::Controller::GetButton(const Binding button) const
{
	const auto gamepad = SDL_GetGamepadFromID(joystick_instance_id);

	const auto ButtonHeld = [&](const SDL_GamepadButton button)
	{
		return SDL_GetGamepadButton(gamepad, button);
	};

	const auto TriggerHeld = [&](const SDL_GamepadAxis axis)
	{
		return SDL_GetGamepadAxis(gamepad, axis) > 0x7FFF / 8;
	};

	const auto StickHeld = [&](const unsigned int direction)
	{
		SDL_assert(direction < 4);

		const auto left_stick_x = SDL_GetGamepadAxis(gamepad, SDL_GAMEPAD_AXIS_LEFTX);
		const auto left_stick_y = SDL_GetGamepadAxis(gamepad, SDL_GAMEPAD_AXIS_LEFTY);
		// Now that we have the left stick's X and Y values, let's do some trigonometry to figure out which direction(s) it's pointing in.

		// To start with, let's treat the X and Y values as a vector, and turn it into a unit vector.
		const float magnitude = std::sqrt(static_cast<float>(left_stick_x * left_stick_x + left_stick_y * left_stick_y));

		const float left_stick_x_unit = left_stick_x / magnitude;
		const float left_stick_y_unit = left_stick_y / magnitude;

		// Now that we have the stick's direction in the form of a unit vector,
		// we can create a dot product of it with another directional unit vector
		// to determine the angle between them.

		// Apply a deadzone.
		if (magnitude < 32768.0f / 4.0f)
			return false;

		// This is a list of directions expressed as unit vectors.
		static const std::array<std::array<float, 2>, 4> directions = {{
			{ 0.0f, -1.0f}, // Up
			{ 0.0f,  1.0f}, // Down
			{-1.0f,  0.0f}, // Left
			{ 1.0f,  0.0f}  // Right
		}};

		// Perform dot product of stick's direction vector with other direction vector.
		const float delta_angle = std::acos(left_stick_x_unit * directions[direction][0] + left_stick_y_unit * directions[direction][1]);

		// If the stick is within 67.5 degrees of the specified direction, then this will be true.
		return delta_angle < CC_DEGREE_TO_RADIAN(360.0f * 3.0f / 8.0f / 2.0f); // Half of 3/8 of 360 degrees (in radians)
	};

	switch (button)
	{
		case Binding::CONTROLLER_UP:
			return ButtonHeld(SDL_GAMEPAD_BUTTON_DPAD_UP) + StickHeld(0);

		case Binding::CONTROLLER_DOWN:
			return ButtonHeld(SDL_GAMEPAD_BUTTON_DPAD_DOWN) + StickHeld(1);

		case Binding::CONTROLLER_LEFT:
			return ButtonHeld(SDL_GAMEPAD_BUTTON_DPAD_LEFT) + StickHeld(2);

		case Binding::CONTROLLER_RIGHT:
			return ButtonHeld(SDL_GAMEPAD_BUTTON_DPAD_RIGHT) + StickHeld(3);

		case Binding::CONTROLLER_A:
			switch (layout)
			{
				case Layout::FOUR_BUTTON:
					return ButtonHeld(SDL_GAMEPAD_BUTTON_WEST);
				case Layout::SIX_BUTTON:
					return ButtonHeld(SDL_GAMEPAD_BUTTON_SOUTH);
			}
			break;

		case Binding::CONTROLLER_B:
			switch (layout)
			{
				case Layout::FOUR_BUTTON:
					return ButtonHeld(SDL_GAMEPAD_BUTTON_SOUTH);
				case Layout::SIX_BUTTON:
					return ButtonHeld(SDL_GAMEPAD_BUTTON_EAST);
			}
			break;

		case Binding::CONTROLLER_C:
			switch (layout)
			{
				case Layout::FOUR_BUTTON:
					return ButtonHeld(SDL_GAMEPAD_BUTTON_EAST);
				case Layout::SIX_BUTTON:
					return TriggerHeld(SDL_GAMEPAD_AXIS_RIGHT_TRIGGER);
			}
			break;

		case Binding::CONTROLLER_X:
			switch (layout)
			{
				case Layout::FOUR_BUTTON:
					return ButtonHeld(SDL_GAMEPAD_BUTTON_LEFT_SHOULDER);
				case Layout::SIX_BUTTON:
					return ButtonHeld(SDL_GAMEPAD_BUTTON_WEST);
			}
			break;

		case Binding::CONTROLLER_Y:
			return ButtonHeld(SDL_GAMEPAD_BUTTON_NORTH);

		case Binding::CONTROLLER_Z:
			return ButtonHeld(SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER);

		case Binding::CONTROLLER_START:
			return ButtonHeld(SDL_GAMEPAD_BUTTON_START);

		case Binding::CONTROLLER_MODE:
			return ButtonHeld(SDL_GAMEPAD_BUTTON_BACK);

		case Binding::REWIND:
			switch (layout)
			{
				case Layout::FOUR_BUTTON:
					return TriggerHeld(SDL_GAMEPAD_AXIS_LEFT_TRIGGER);
				case Layout::SIX_BUTTON:
					return ButtonHeld(SDL_GAMEPAD_BUTTON_LEFT_SHOULDER);
			}
			break;

		case Binding::FAST_FORWARD:
			switch (layout)
			{
				case Layout::FOUR_BUTTON:
					return TriggerHeld(SDL_GAMEPAD_AXIS_RIGHT_TRIGGER);
				case Layout::SIX_BUTTON:
					return TriggerHeld(SDL_GAMEPAD_AXIS_LEFT_TRIGGER);
			}
			break;

		default:
			// Not bound.
			break;
	}

	return 0;
}
