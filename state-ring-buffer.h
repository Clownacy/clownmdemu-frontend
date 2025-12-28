#ifndef STATE_RING_BUFFER_H
#define STATE_RING_BUFFER_H

#include <array>
#include <cassert>
#include <vector>

template<typename Emulator>
class StateRingBuffer
{
protected:
	std::vector<std::array<std::byte, sizeof(typename Emulator::State)>> state_buffer;
	std::size_t state_buffer_index = 0;
	std::size_t state_buffer_remaining = 0;

	std::size_t NextIndex(std::size_t index) const
	{
		++index;

		if (index == std::size(state_buffer))
			index = 0;

		return index;
	}

	std::size_t PreviousIndex(std::size_t index) const
	{
		if (index == 0)
			index = std::size(state_buffer);

		--index;

		return index;
	}

public:
	StateRingBuffer(const bool enabled)
		: state_buffer(enabled ? 10 * 60 : 0) // 10 seconds
	{}

	void Clear()
	{
		state_buffer_index = state_buffer_remaining = 0;
	}

	[[nodiscard]] bool Exhausted() const
	{
		// We need at least two frames, because rewinding pops one frame and then samples the frame below the head.
		return state_buffer_remaining < 2;
	}

	void Push(Emulator &emulator)
	{
		assert(Exists());

		state_buffer_remaining = std::min(state_buffer_remaining + 1, std::size(state_buffer) - 2);

		const auto old_index = state_buffer_index;
		state_buffer_index = NextIndex(state_buffer_index);
		new(&state_buffer[old_index]) Emulator::State(emulator);
	}

	void Pop(Emulator &emulator)
	{
		assert(Exists());
		assert(!Exhausted());
		--state_buffer_remaining;

		state_buffer_index = PreviousIndex(state_buffer_index);
		auto &state = *reinterpret_cast<Emulator::State*>(&state_buffer[PreviousIndex(state_buffer_index)]);
		state.Apply(emulator);
		state.~State();
	}

	[[nodiscard]] bool Exists() const
	{
		return !state_buffer.empty();
	}
};

#endif // STATE_RING_BUFFER_H
