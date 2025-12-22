#ifndef CLOWNMDEMU_CPP_H
#define CLOWNMDEMU_CPP_H

#include <cassert>
#include <cstdarg>
#include <cstddef>
#include <cstdio>
#include <functional>
#include <string>

#include "common/core/clownmdemu.h"

namespace ClownMDEmuCPP
{
	class Constant
	{
	public:
		Constant()
		{
			ClownMDEmu_Constant_Initialise();
		}
	};

	class Configuration : public ClownMDEmu_Configuration
	{
	public:
		Configuration()
			: ClownMDEmu_Configuration({})
		{}
	};

	class State : public ClownMDEmu_State
	{
	public:
		State()
		{
			ClownMDEmu_State_Initialise(this);
		}
	};

	inline Constant constant;

	template<typename Derived>
	class Base
	{
	public:
		using LogCallbackFormatted = std::function<void(const char *format, std::va_list arg)>;
		using LogCallbackPlain = std::function<void(const std::string &message)>;

	protected:
		class Parameters
		{
		protected:
			ClownMDEmu clownmdemu;

		public:
			Parameters(const ClownMDEmu_Configuration &configuration, ClownMDEmu_State &state, const ClownMDEmu_Callbacks &callbacks)
				: clownmdemu(CLOWNMDEMU_PARAMETERS_INITIALISE(&configuration, &state, &callbacks))
			{}

			const ClownMDEmu& operator*() const
			{
				return clownmdemu;
			}

			ClownMDEmu& operator*()
			{
				return clownmdemu;
			}

			const ClownMDEmu* operator->() const
			{
				return &**this;
			}

			ClownMDEmu* operator->()
			{
				return &**this;
			}
		};

		const ClownMDEmu_Callbacks callbacks = {
			this,
			Callback_ColourUpdated,
			Callback_ScanlineRendered,
			Callback_InputRequested,
			Callback_FMAudioToBeGenerated,
			Callback_PSGAudioToBeGenerated,
			Callback_PCMAudioToBeGenerated,
			Callback_CDDAAudioToBeGenerated,
			Callback_CDSeeked,
			Callback_CDSectorRead,
			Callback_CDTrackSeeked,
			Callback_CDAudioRead,
			Callback_SaveFileOpenedForReading,
			Callback_SaveFileRead,
			Callback_SaveFileOpenedForWriting,
			Callback_SaveFileWritten,
			Callback_SaveFileClosed,
			Callback_SaveFileRemoved,
			Callback_SaveFileSizeObtained
		};

		State state;
		Parameters parameters;
		LogCallbackFormatted log_callback;

		static void Callback_ColourUpdated(void* const user_data, const cc_u16f index, const cc_u16f colour)
		{
			static_cast<Derived*>(static_cast<Base*>(user_data))->ColourUpdated(index, colour);
		}

		static void Callback_ScanlineRendered(void* const user_data, const cc_u16f scanline, const cc_u8l* const pixels, const cc_u16f left_boundary, const cc_u16f right_boundary, const cc_u16f screen_width, const cc_u16f screen_height)
		{
			static_cast<Derived*>(static_cast<Base*>(user_data))->ScanlineRendered(scanline, pixels, left_boundary, right_boundary, screen_width, screen_height);
		}

		static cc_bool Callback_InputRequested(void* const user_data, const cc_u8f player_id, const ClownMDEmu_Button button_id)
		{
			return static_cast<Derived*>(static_cast<Base*>(user_data))->InputRequested(player_id, button_id);
		}

		static void Callback_FMAudioToBeGenerated(void* const user_data, const ClownMDEmu* const clownmdemu, const std::size_t total_frames, void (* const generate_fm_audio)(const ClownMDEmu *clownmdemu, cc_s16l *sample_buffer, std::size_t total_frames))
		{
			static_cast<Derived*>(static_cast<Base*>(user_data))->FMAudioToBeGenerated(clownmdemu, total_frames, generate_fm_audio);
		}

		static void Callback_PSGAudioToBeGenerated(void* const user_data, const ClownMDEmu* const clownmdemu, const std::size_t total_frames, void (* const generate_psg_audio)(const ClownMDEmu *clownmdemu, cc_s16l *sample_buffer, std::size_t total_frames))
		{
			static_cast<Derived*>(static_cast<Base*>(user_data))->PSGAudioToBeGenerated(clownmdemu, total_frames, generate_psg_audio);
		}

		static void Callback_PCMAudioToBeGenerated(void* const user_data, const ClownMDEmu* const clownmdemu, const std::size_t total_frames, void (* const generate_pcm_audio)(const ClownMDEmu *clownmdemu, cc_s16l *sample_buffer, std::size_t total_frames))
		{
			static_cast<Derived*>(static_cast<Base*>(user_data))->PCMAudioToBeGenerated(clownmdemu, total_frames, generate_pcm_audio);
		}

		static void Callback_CDDAAudioToBeGenerated(void* const user_data, const ClownMDEmu* const clownmdemu, const std::size_t total_frames, void (* const generate_cdda_audio)(const ClownMDEmu *clownmdemu, cc_s16l *sample_buffer, std::size_t total_frames))
		{
			static_cast<Derived*>(static_cast<Base*>(user_data))->CDDAAudioToBeGenerated(clownmdemu, total_frames, generate_cdda_audio);
		}

		static void Callback_CDSeeked(void* const user_data, const cc_u32f sector_index)
		{
			static_cast<Derived*>(static_cast<Base*>(user_data))->CDSeeked(sector_index);
		}

		static void Callback_CDSectorRead(void* const user_data, cc_u16l* const buffer)
		{
			static_cast<Derived*>(static_cast<Base*>(user_data))->CDSectorRead(buffer);
		}

		static cc_bool Callback_CDTrackSeeked(void* const user_data, const cc_u16f track_index, const ClownMDEmu_CDDAMode mode)
		{
			return static_cast<Derived*>(static_cast<Base*>(user_data))->CDTrackSeeked(track_index, mode);
		}

		static std::size_t Callback_CDAudioRead(void* const user_data, cc_s16l* const sample_buffer, const std::size_t total_frames)
		{
			return static_cast<Derived*>(static_cast<Base*>(user_data))->CDAudioRead(sample_buffer, total_frames);
		}

		static cc_bool Callback_SaveFileOpenedForReading(void* const user_data, const char* const filename)
		{
			return static_cast<Derived*>(static_cast<Base*>(user_data))->SaveFileOpenedForReading(filename);
		}

		static cc_s16f Callback_SaveFileRead(void* const user_data)
		{
			return static_cast<Derived*>(static_cast<Base*>(user_data))->SaveFileRead();
		}

		static cc_bool Callback_SaveFileOpenedForWriting(void* const user_data, const char* const filename)
		{
			return static_cast<Derived*>(static_cast<Base*>(user_data))->SaveFileOpenedForWriting(filename);
		}

		static void Callback_SaveFileWritten(void* const user_data, const cc_u8f byte)
		{
			static_cast<Derived*>(static_cast<Base*>(user_data))->SaveFileWritten(byte);
		}

		static void Callback_SaveFileClosed(void* const user_data)
		{
			static_cast<Derived*>(static_cast<Base*>(user_data))->SaveFileClosed();
		}

		static cc_bool Callback_SaveFileRemoved(void* const user_data, const char* const filename)
		{
			return static_cast<Derived*>(static_cast<Base*>(user_data))->SaveFileRemoved(filename);
		}

		static cc_bool Callback_SaveFileSizeObtained(void* const user_data, const char* const filename, std::size_t* const size)
		{
			return static_cast<Derived*>(static_cast<Base*>(user_data))->SaveFileSizeObtained(filename, size);
		}

		static void LogCallbackWrapper(void* const user_data, const char *format, std::va_list arg)
		{
			(*static_cast<const LogCallbackFormatted*>(user_data))(format, arg);
		}

	public:
		Base(const Configuration &configuration)
			: parameters(configuration, state, callbacks)
		{}

		void SetCartridge(const cc_u16l* const buffer, const cc_u32f buffer_length)
		{
			ClownMDEmu_SetCartridge(&*parameters, buffer, buffer_length);
		}

		void SoftReset(const cc_bool cartridge_inserted, const cc_bool cd_inserted)
		{
			ClownMDEmu_Reset(&*parameters, cartridge_inserted, cd_inserted);
		}

		void HardReset(const cc_bool cartridge_inserted, const cc_bool cd_inserted)
		{
			state = State();
			SoftReset(cartridge_inserted, cd_inserted);
		}

		void Iterate()
		{
			ClownMDEmu_Iterate(&*parameters);
		}

		void SetLogCallback(const LogCallbackFormatted &callback)
		{
			log_callback = callback;
			ClownMDEmu_SetLogCallback(&LogCallbackWrapper, &log_callback);
		}

		void SetLogCallback(const LogCallbackPlain &callback)
		{
			SetLogCallback(
				[=](const char* const format, std::va_list arg)
				{
					// TODO: Use 'std::string::resize_and_overwrite' here.
					std::va_list arg_copy;
					va_copy(arg_copy, arg);
					std::string string(std::vsnprintf(nullptr, 0, format, arg_copy), '\0');
					va_end(arg_copy);

					std::vsnprintf(std::data(string), std::size(string) + 1, format, arg);
					callback(string);
				}
			);
		}

		[[nodiscard]] const auto& GetState() const
		{
			return state;
		}

		void SetState(const State &state)
		{
			this->state = state;
		}

		[[nodiscard]] auto& GetExternalRAM()
		{
			return state.external_ram.buffer;
		}

		[[nodiscard]] bool InterlaceMode2Enabled() const
		{
			return parameters->state->vdp.double_resolution_enabled;
		}
	};
}

#endif // CLOWNMDEMU_CPP_H
