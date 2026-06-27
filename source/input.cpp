#include "input.h"

#include "../libraries/imgui/imgui.h"

KeyboardInput keyboard_input = {"Keyboard"};
std::list<ControllerInput> controller_input_list;

std::array<const Input*, 8> bound_inputs;

ControllerLayout controller_layout;

unsigned int Input::GetButton(const InputBinding button) const
{
	const unsigned int total_presses = GetButtonInternal(button);

	if (total_presses != 0)
	{
		// Automatically bind this controller to the first unbound input.
		for (auto &bound_input : bound_inputs)
			if (bound_input == this)
				return total_presses;

		for (auto &bound_input : bound_inputs)
		{
			if (bound_input == nullptr)
			{
				bound_input = this;
				break;
			}
		}
	}

	return total_presses;
}
unsigned int KeyboardInput::GetButtonInternal(const InputBinding button) const
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

unsigned int ControllerInput::GetButtonInternal(const InputBinding button) const
{
	// Don't use inputs that are for Dear ImGui.
	if ((ImGui::GetIO().ConfigFlags & ImGuiConfigFlags_NavEnableGamepad) != 0)
		return 0;

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
		case InputBinding::CONTROLLER_UP:
			return ButtonHeld(SDL_GAMEPAD_BUTTON_DPAD_UP) + StickHeld(0);

		case InputBinding::CONTROLLER_DOWN:
			return ButtonHeld(SDL_GAMEPAD_BUTTON_DPAD_DOWN) + StickHeld(1);

		case InputBinding::CONTROLLER_LEFT:
			return ButtonHeld(SDL_GAMEPAD_BUTTON_DPAD_LEFT) + StickHeld(2);

		case InputBinding::CONTROLLER_RIGHT:
			return ButtonHeld(SDL_GAMEPAD_BUTTON_DPAD_RIGHT) + StickHeld(3);

		case InputBinding::CONTROLLER_A:
			switch (controller_layout)
			{
				case ControllerLayout::FOUR_BUTTON:
					return ButtonHeld(SDL_GAMEPAD_BUTTON_WEST);
				case ControllerLayout::SIX_BUTTON:
					return ButtonHeld(SDL_GAMEPAD_BUTTON_SOUTH);
			}
			break;

		case InputBinding::CONTROLLER_B:
			switch (controller_layout)
			{
				case ControllerLayout::FOUR_BUTTON:
					return ButtonHeld(SDL_GAMEPAD_BUTTON_SOUTH);
				case ControllerLayout::SIX_BUTTON:
					return ButtonHeld(SDL_GAMEPAD_BUTTON_EAST);
			}
			break;

		case InputBinding::CONTROLLER_C:
			switch (controller_layout)
			{
				case ControllerLayout::FOUR_BUTTON:
					return ButtonHeld(SDL_GAMEPAD_BUTTON_EAST);
				case ControllerLayout::SIX_BUTTON:
					return TriggerHeld(SDL_GAMEPAD_AXIS_RIGHT_TRIGGER);
			}
			break;

		case InputBinding::CONTROLLER_X:
			switch (controller_layout)
			{
				case ControllerLayout::FOUR_BUTTON:
					return ButtonHeld(SDL_GAMEPAD_BUTTON_LEFT_SHOULDER);
				case ControllerLayout::SIX_BUTTON:
					return ButtonHeld(SDL_GAMEPAD_BUTTON_WEST);
			}
			break;

		case InputBinding::CONTROLLER_Y:
			return ButtonHeld(SDL_GAMEPAD_BUTTON_NORTH);

		case InputBinding::CONTROLLER_Z:
			return ButtonHeld(SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER);

		case InputBinding::CONTROLLER_START:
			return ButtonHeld(SDL_GAMEPAD_BUTTON_START);

		case InputBinding::CONTROLLER_MODE:
			return ButtonHeld(SDL_GAMEPAD_BUTTON_BACK);

		case InputBinding::REWIND:
			switch (controller_layout)
			{
				case ControllerLayout::FOUR_BUTTON:
					return TriggerHeld(SDL_GAMEPAD_AXIS_LEFT_TRIGGER);
				case ControllerLayout::SIX_BUTTON:
					return ButtonHeld(SDL_GAMEPAD_BUTTON_LEFT_SHOULDER);
			}
			break;

		case InputBinding::FAST_FORWARD:
			switch (controller_layout)
			{
				case ControllerLayout::FOUR_BUTTON:
					return TriggerHeld(SDL_GAMEPAD_AXIS_RIGHT_TRIGGER);
				case ControllerLayout::SIX_BUTTON:
					return TriggerHeld(SDL_GAMEPAD_AXIS_LEFT_TRIGGER);
			}
			break;
	}

	return 0;
}
