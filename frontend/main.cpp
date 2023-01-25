#include <stdarg.h>
#include <stddef.h>

#include "SDL.h"

#include "../clowncommon.h"
#include "../clownmdemu.h"

#include "libraries/tinyfiledialogs/tinyfiledialogs.h"

#include "libraries/imgui/imgui.h"
#include "libraries/imgui/backends/imgui_impl_sdl.h"
#include "libraries/imgui/backends/imgui_impl_sdlrenderer.h"

#include "inconsolata_regular.h"
#include "karla_regular.h"

#include "error.h"
#include "debug_m68k.h"
#include "debug_memory.h"
#include "debug_fm.h"
#include "debug_psg.h"
#include "debug_vdp.h"

#define MIXER_FORMAT Sint16
#include "mixer.c"

typedef struct Input
{
	unsigned int bound_joypad;
	bool buttons[CLOWNMDEMU_BUTTON_MAX];
	bool fast_forward;
	bool rewind;
} Input;

typedef struct ControllerInput
{
	SDL_JoystickID joystick_instance_id;
	Sint16 left_stick_x;
	Sint16 left_stick_y;
	bool left_stick[4];
	bool dpad[4];
	Input input;

	struct ControllerInput *next;
} ControllerInput;

typedef struct EmulationState
{
	ClownMDEmu_State clownmdemu;
	Uint32 colours[3 * 4 * 16];
} EmulationState;

static Input keyboard_input;

static ControllerInput *controller_input_list_head;

#ifdef CLOWNMDEMUFRONTEND_REWINDING
static EmulationState state_rewind_buffer[60 * 10]; // Roughly ten seconds of rewinding at 60FPS
static size_t state_rewind_index;
static size_t state_rewind_remaining;
#else
static EmulationState state_rewind_buffer[1];
#endif
static EmulationState *emulation_state = state_rewind_buffer;

static bool quick_save_exists = false;
static EmulationState quick_save_state;

static unsigned char *rom_buffer;
static size_t rom_buffer_size;

static ClownMDEmu_Configuration clownmdemu_configuration;
static ClownMDEmu_Constant clownmdemu_constant;
static ClownMDEmu clownmdemu;

static void LoadFileToBuffer(const char *filename, unsigned char **file_buffer, size_t *file_size)
{
	*file_buffer = NULL;
	*file_size = 0;

	SDL_RWops *file = SDL_RWFromFile(filename, "rb");

	if (file == NULL)
	{
		PrintError("SDL_RWFromFile failed with the following message - '%s'", SDL_GetError());
	}
	else
	{
		const Sint64 size_s64 = SDL_RWsize(file);

		if (size_s64 < 0)
		{
			PrintError("SDL_RWsize failed with the following message - '%s'", SDL_GetError());
		}
		else
		{
			const size_t size = (size_t)size_s64;

			*file_buffer = (unsigned char*)SDL_malloc(size);

			if (*file_buffer == NULL)
			{
				PrintError("Could not allocate memory for file");
			}
			else
			{
				*file_size = size;

				SDL_RWread(file, *file_buffer, 1, size);
			}
		}

		if (SDL_RWclose(file) < 0)
			PrintError("SDL_RWclose failed with the following message - '%s'", SDL_GetError());
	}
}


///////////
// Video //
///////////

#define FRAMEBUFFER_WIDTH 320
#define FRAMEBUFFER_HEIGHT (240*2) // *2 because of double-resolution mode.

static SDL_Window *window;
static SDL_Renderer *renderer;

static SDL_Texture *framebuffer_texture;
static Uint32 *framebuffer_texture_pixels;
static int framebuffer_texture_pitch;
static SDL_Texture *framebuffer_texture_upscaled;
static unsigned int framebuffer_texture_upscaled_width;
static unsigned int framebuffer_texture_upscaled_height;

static unsigned int current_screen_width;
static unsigned int current_screen_height;

static bool use_vsync;
static bool fullscreen;
static float dpi_scale;

static float GetNewDPIScale(void)
{
	float dpi_scale = 1.0f;

	float ddpi;
	if (SDL_GetDisplayDPI(SDL_GetWindowDisplayIndex(window), &ddpi, NULL, NULL) == 0)
		dpi_scale = ddpi / 96.0f;

	return dpi_scale;
}

static bool InitialiseVideo(void)
{
	if (SDL_InitSubSystem(SDL_INIT_VIDEO) < 0)
	{
		PrintError("SDL_InitSubSystem(SDL_INIT_VIDEO) failed with the following message - '%s'", SDL_GetError());
	}
	else
	{
		// Create window
		window = SDL_CreateWindow("clownmdemufrontend", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, 320 * 2, 224 * 2, SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI | SDL_WINDOW_HIDDEN);

		if (window == NULL)
		{
			PrintError("SDL_CreateWindow failed with the following message - '%s'", SDL_GetError());
		}
		else
		{
			// Use batching even if the user forces a specific rendering backend (wtf SDL).
			//
			// Personally, I like to force SDL2 to use D3D11 instead of D3D9 by setting an environment
			// variable, but it turns out that, if I do that, then SDL2 will disable its render batching
			// for backwards compatibility reasons. Setting this hint prevents that.
			//
			// Normally if a user is setting an environment variable to force a specific rendering
			// backend, then they're expected to set another environment variable to set this hint too,
			// but I might as well just do it myself and save everyone else the hassle.
			SDL_SetHint(SDL_HINT_RENDER_BATCHING, "1");

			// Create renderer
			renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_TARGETTEXTURE);

			if (renderer == NULL)
			{
				PrintError("SDL_CreateRenderer failed with the following message - '%s'", SDL_GetError());
			}
			else
			{
				dpi_scale = GetNewDPIScale();

				return true;
			}

			SDL_DestroyWindow(window);
		}

		SDL_QuitSubSystem(SDL_INIT_VIDEO);
	}

	return false;
}

static void DeinitialiseVideo(void)
{
	SDL_DestroyRenderer(renderer);
	SDL_DestroyWindow(window);
	SDL_QuitSubSystem(SDL_INIT_VIDEO);
}

static void SetFullscreen(bool enabled)
{
	SDL_SetWindowFullscreen(window, enabled ? SDL_WINDOW_FULLSCREEN_DESKTOP : 0);
}

static bool InitialiseFramebuffer(void)
{
	// Create framebuffer texture
	// We're using ARGB8888 because it's more likely to be supported natively by the GPU, avoiding the need for constant conversions
	SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "nearest");
	framebuffer_texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, FRAMEBUFFER_WIDTH, FRAMEBUFFER_HEIGHT);

	if (framebuffer_texture == NULL)
	{
		PrintError("SDL_CreateTexture failed with the following message - '%s'", SDL_GetError());
	}
	else
	{
		// Disable blending, since we don't need it
		if (SDL_SetTextureBlendMode(framebuffer_texture, SDL_BLENDMODE_NONE) < 0)
			PrintError("SDL_SetTextureBlendMode failed with the following message - '%s'", SDL_GetError());

		return true;
	}

	return false;
}

static void DeinitialiseFramebuffer(void)
{
	SDL_DestroyTexture(framebuffer_texture);
}

static void RecreateUpscaledFramebuffer(unsigned int display_width, unsigned int display_height)
{
	static unsigned int previous_framebuffer_size_factor = 0;

	const unsigned int framebuffer_size_factor = CC_MAX(1, CC_MIN(CC_DIVIDE_CEILING(display_width, 640), CC_DIVIDE_CEILING(display_height, 480)));

	if (framebuffer_texture_upscaled == NULL || framebuffer_size_factor != previous_framebuffer_size_factor)
	{
		previous_framebuffer_size_factor = framebuffer_size_factor;

		framebuffer_texture_upscaled_width = 640 * framebuffer_size_factor;
		framebuffer_texture_upscaled_height = 480 * framebuffer_size_factor;

		SDL_DestroyTexture(framebuffer_texture_upscaled); // It should be safe to pass NULL to this
		SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "linear");
		framebuffer_texture_upscaled = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_TARGET, framebuffer_texture_upscaled_width, framebuffer_texture_upscaled_height);

		if (framebuffer_texture_upscaled == NULL)
		{
			PrintError("SDL_CreateTexture failed with the following message - '%s'", SDL_GetError());
		}
		else
		{
			// Disable blending, since we don't need it
			if (SDL_SetTextureBlendMode(framebuffer_texture_upscaled, SDL_BLENDMODE_NONE) < 0)
				PrintError("SDL_SetTextureBlendMode failed with the following message - '%s'", SDL_GetError());
		}
	}
}


///////////
// Audio //
///////////

static SDL_AudioDeviceID audio_device;
static Uint32 audio_device_buffer_size;
static unsigned long audio_device_sample_rate;
static bool pal_mode;
static bool low_pass_filter = true;

static Mixer_Constant mixer_constant;
static Mixer_State mixer_state;
static const Mixer mixer = {&mixer_constant, &mixer_state};

static bool InitialiseAudio(void)
{
	SDL_AudioSpec want, have;

	SDL_zero(want);
	want.freq = 48000;
	want.format = AUDIO_S16;
	want.channels = MIXER_FM_CHANNEL_COUNT;
	// We want a 10ms buffer (this value must be a power of two).
	want.samples = 1;
	while (want.samples < (want.freq * want.channels) / (1000 / 10))
		want.samples <<= 1;
	want.callback = NULL;

	audio_device = SDL_OpenAudioDevice(NULL, 0, &want, &have, SDL_AUDIO_ALLOW_FREQUENCY_CHANGE | SDL_AUDIO_ALLOW_SAMPLES_CHANGE);

	if (audio_device == 0)
	{
		PrintError("SDL_OpenAudioDevice failed with the following message - '%s'", SDL_GetError());
	}
	else
	{
		audio_device_buffer_size = have.size;
		audio_device_sample_rate = (unsigned long)have.freq;

		// Initialise the mixer.
		Mixer_Constant_Initialise(&mixer_constant);
		Mixer_State_Initialise(&mixer_state, audio_device_sample_rate, pal_mode, low_pass_filter);

		// Unpause audio device, so that playback can begin.
		SDL_PauseAudioDevice(audio_device, 0);

		return true;
	}

	return false;
}

static void DeinitialiseAudio(void)
{
	if (audio_device != 0)
		SDL_CloseAudioDevice(audio_device);
}

static void SetAudioPALMode(bool enabled)
{
	pal_mode = enabled;

	if (audio_device != 0)
		Mixer_State_Initialise(&mixer_state, audio_device_sample_rate, pal_mode, low_pass_filter);
}

static void AudioPushCallback(const void *user_data, Sint16 *audio_samples, size_t total_frames)
{
	(void)user_data;

	SDL_QueueAudio(audio_device, audio_samples, (Uint32)(total_frames * sizeof(Sint16) * MIXER_FM_CHANNEL_COUNT));
}


///////////
// Fonts //
///////////

static ImFont *monospace_font;

static unsigned int CalculateFontSize(void)
{
	// Note that we are purposefully flooring, as Dear ImGui's docs recommend.
	return (unsigned int)(15.0f * dpi_scale);
}

static void ReloadFonts(unsigned int font_size)
{
	ImGuiIO &io = ImGui::GetIO();

	io.Fonts->Clear();
	ImGui_ImplSDLRenderer_DestroyFontsTexture();

	ImFontConfig font_cfg = ImFontConfig();
	SDL_snprintf(font_cfg.Name, IM_ARRAYSIZE(font_cfg.Name), "Karla Regular, %upx", font_size);
	io.Fonts->AddFontFromMemoryCompressedTTF(karla_regular_compressed_data, karla_regular_compressed_size, (float)font_size, &font_cfg);
	SDL_snprintf(font_cfg.Name, IM_ARRAYSIZE(font_cfg.Name), "Inconsolata Regular, %upx", font_size);
	monospace_font = io.Fonts->AddFontFromMemoryCompressedTTF(inconsolata_regular_compressed_data, inconsolata_regular_compressed_size, (float)font_size, &font_cfg);
}


////////////////////////////
// Emulator Functionality //
////////////////////////////

static cc_u16f CartridgeReadCallback(const void *user_data, cc_u32f address)
{
	(void)user_data;

	if (address >= rom_buffer_size)
		return 0;

	return rom_buffer[address];
}

static void CartridgeWrittenCallback(const void *user_data, cc_u32f address, cc_u16f value)
{
	(void)user_data;

	// For now, let's pretend that the cartridge is read-only, like retail cartridges are.
	(void)address;
	(void)value;

	/*
	if (address >= rom_buffer_size)
		return;

	rom_buffer[address] = value;
	*/
}

static void ColourUpdatedCallback(const void *user_data, cc_u16f index, cc_u16f colour)
{
	(void)user_data;

	// Decompose XBGR4444 into individual colour channels
	const cc_u32f red = (colour >> 4 * 0) & 0xF;
	const cc_u32f green = (colour >> 4 * 1) & 0xF;
	const cc_u32f blue = (colour >> 4 * 2) & 0xF;

	// Reassemble into ARGB8888
	emulation_state->colours[index] = (Uint32)((blue << 4 * 0) | (blue << 4 * 1) | (green << 4 * 2) | (green << 4 * 3) | (red << 4 * 4) | (red << 4 * 5));
}

static void ScanlineRenderedCallback(const void *user_data, cc_u16f scanline, const cc_u8l *pixels, cc_u16f screen_width, cc_u16f screen_height)
{
	(void)user_data;

	current_screen_width = screen_width;
	current_screen_height = screen_height;

	if (framebuffer_texture_pixels != NULL)
		for (cc_u16f i = 0; i < screen_width; ++i)
			framebuffer_texture_pixels[scanline * framebuffer_texture_pitch + i] = emulation_state->colours[pixels[i]];
}

static cc_bool ReadInputCallback(const void *user_data, cc_u16f player_id, ClownMDEmu_Button button_id)
{
	(void)user_data;

	SDL_assert(player_id < 2);

	cc_bool value = cc_false;

	// First, try to obtain the input from the keyboard.
	if (keyboard_input.bound_joypad == player_id)
		value |= keyboard_input.buttons[button_id] ? cc_true : cc_false;

	// Then, try to obtain the input from the controllers.
	for (ControllerInput *controller_input = controller_input_list_head; controller_input != NULL; controller_input = controller_input->next)
		if (controller_input->input.bound_joypad == player_id)
			value |= controller_input->input.buttons[button_id] ? cc_true : cc_false;

	return value;
}

static void FMAudioCallback(const void *user_data, size_t total_frames, void (*generate_fm_audio)(const ClownMDEmu *clownmdemu, cc_s16l *sample_buffer, size_t total_frames))
{
	(void)user_data;

	generate_fm_audio(&clownmdemu, Mixer_AllocateFMSamples(&mixer, total_frames), total_frames);
}

static void PSGAudioCallback(const void *user_data, size_t total_samples, void (*generate_psg_audio)(const ClownMDEmu *clownmdemu, cc_s16l *sample_buffer, size_t total_samples))
{
	(void)user_data;

	generate_psg_audio(&clownmdemu, Mixer_AllocatePSGSamples(&mixer, total_samples), total_samples);
}

static void ApplySaveState(EmulationState *save_state)
{
	*emulation_state = *save_state;
}

static void OpenSoftware(const char *path, const ClownMDEmu_Callbacks *callbacks)
{
	unsigned char *temp_rom_buffer;
	size_t temp_rom_buffer_size;

	// Load ROM to memory.
	LoadFileToBuffer(path, &temp_rom_buffer, &temp_rom_buffer_size);

	if (temp_rom_buffer == NULL)
	{
		PrintError("Could not load the software");
		SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Error", "Failed to load the software.", window);
	}
	else
	{
		// Unload the previous ROM in memory.
		SDL_free(rom_buffer);

		quick_save_exists = false;

		rom_buffer = temp_rom_buffer;
		rom_buffer_size = temp_rom_buffer_size;

	#ifdef CLOWNMDEMUFRONTEND_REWINDING
		state_rewind_remaining = 0;
		state_rewind_index = 0;
	#endif
		emulation_state = &state_rewind_buffer[0];

		ClownMDEmu_State_Initialise(&emulation_state->clownmdemu);
		ClownMDEmu_Parameters_Initialise(&clownmdemu, &clownmdemu_configuration, &clownmdemu_constant, &emulation_state->clownmdemu);
		ClownMDEmu_Reset(&clownmdemu, callbacks);
	}
}

// Manages whether the emulator runs at a higher speed or not.
static bool fast_forward_in_progress;

static void UpdateFastForwardStatus(void)
{
	const bool previous_fast_forward_in_progress = fast_forward_in_progress;

	fast_forward_in_progress = keyboard_input.fast_forward;

	for (ControllerInput *controller_input = controller_input_list_head; controller_input != NULL; controller_input = controller_input->next)
		fast_forward_in_progress |= controller_input->input.fast_forward;

	if (previous_fast_forward_in_progress != fast_forward_in_progress)
	{
		// Disable V-sync so that 60Hz displays aren't locked to 1x speed while fast-forwarding
		if (use_vsync)
			SDL_RenderSetVSync(renderer, !fast_forward_in_progress);
	}
}

#ifdef CLOWNMDEMUFRONTEND_REWINDING
static bool rewind_in_progress;

static void UpdateRewindStatus(void)
{
	rewind_in_progress = keyboard_input.rewind;

	for (ControllerInput *controller_input = controller_input_list_head; controller_input != NULL; controller_input = controller_input->next)
		rewind_in_progress |= controller_input->input.rewind;
}
#endif

static const char* OpenFileDialog(char const *aTitle, char const *aDefaultPathAndFile, int aNumOfFilterPatterns, const char* const *aFilterPatterns, char const *aSingleFilterDescription, int aAllowMultipleSelects)
{
	// A workaround to prevent the dialog being impossible to close in fullscreen (at least on Windows).
	if (fullscreen)
		SetFullscreen(false);

	const char *path = tinyfd_openFileDialog(aTitle, aDefaultPathAndFile, aNumOfFilterPatterns, aFilterPatterns, aSingleFilterDescription, aAllowMultipleSelects);

	if (fullscreen)
		SetFullscreen(true);

	return path;
}

static const char* SaveFileDialog(char const *aTitle, char const *aDefaultPathAndFile, int aNumOfFilterPatterns, const char* const *aFilterPatterns, char const *aSingleFilterDescription)
{
	// A workaround to prevent the dialog being impossible to close in fullscreen (at least on Windows).
	if (fullscreen)
		SetFullscreen(false);

	const char *path = tinyfd_saveFileDialog(aTitle, aDefaultPathAndFile, aNumOfFilterPatterns, aFilterPatterns, aSingleFilterDescription);

	if (fullscreen)
		SetFullscreen(true);

	return path;
}

int main(int argc, char **argv)
{
	InitError();

	// Initialise SDL2
	if (SDL_Init(SDL_INIT_AUDIO | SDL_INIT_EVENTS | SDL_INIT_GAMECONTROLLER) < 0)
	{
		PrintError("SDL_Init failed with the following message - '%s'", SDL_GetError());
		SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Fatal Error", "Unable to initialise SDL2. The program will now close.", NULL);
	}
	else
	{
		if (!InitialiseVideo())
		{
			PrintError("InitVideo failed");
			SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Fatal Error", "Unable to initialise video subsystem. The program will now close.", NULL);
		}
		else
		{
			if (!InitialiseFramebuffer())
			{
				PrintError("CreateFramebuffer failed");
				SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Fatal Error", "Unable to initialise framebuffer. The program will now close.", NULL);
			}
			else
			{
				const int display_index = SDL_GetWindowDisplayIndex(window);

				if (display_index >= 0)
				{
					SDL_DisplayMode display_mode;

					if (SDL_GetCurrentDisplayMode(display_index, &display_mode) == 0)
					{
						// Enable V-sync on displays with an FPS of a multiple of 60.
						use_vsync = display_mode.refresh_rate % 60 == 0;
						SDL_RenderSetVSync(renderer, use_vsync);
					}
				}

				// Setup Dear ImGui context
				IMGUI_CHECKVERSION();
				ImGui::CreateContext();
				ImGuiIO &io = ImGui::GetIO();
				ImGuiStyle &style = ImGui::GetStyle();
				io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls

				// Setup Dear ImGui style
				ImGui::StyleColorsDark();
				//ImGui::StyleColorsLight();
				//ImGui::StyleColorsClassic();

				// Apply DPI scale (also resize the window so that there's room for the menu bar).
				style.ScaleAllSizes(dpi_scale);
				const unsigned int font_size = CalculateFontSize();
				const float menu_bar_size = (float)font_size + style.FramePadding.y * 2.0f; // An inlined ImGui::GetFrameHeight that actually works
				SDL_SetWindowSize(window, (int)(320.0f * 2.0f * dpi_scale), (int)(224.0f * 2.0f * dpi_scale + menu_bar_size));

				// We are now ready to show the window
				SDL_ShowWindow(window);

				// Setup Platform/Renderer backends
				ImGui_ImplSDL2_InitForSDLRenderer(window, renderer);
				ImGui_ImplSDLRenderer_Init(renderer);

				// Load fonts
				ReloadFonts(font_size);

				// This should be called before any other clownmdemu functions are called!
				ClownMDEmu_SetErrorCallback(PrintErrorInternal);

				// Initialise the clownmdemu configuration struct.
				clownmdemu_configuration.general.region = CLOWNMDEMU_REGION_OVERSEAS;
				clownmdemu_configuration.general.tv_standard = CLOWNMDEMU_TV_STANDARD_NTSC;
				clownmdemu_configuration.vdp.sprites_disabled = cc_false;
				clownmdemu_configuration.vdp.planes_disabled[0] = cc_false;
				clownmdemu_configuration.vdp.planes_disabled[1] = cc_false;

				// Initialise persistent data such as lookup tables.
				ClownMDEmu_Constant_Initialise(&clownmdemu_constant);

				// Intiialise audio if we can (but it's okay if it fails).
				if (!InitialiseAudio())
				{
					PrintError("FM CreateAudioDevice failed");
					SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_WARNING, "Warning", "Unable to initialise audio subsystem: the program will not output audio!", window);
				}
				else
				{
					// Initialise resamplers.
					SetAudioPALMode(clownmdemu_configuration.general.tv_standard == CLOWNMDEMU_TV_STANDARD_PAL);
				}

				// Construct our big list of callbacks for clownmdemu.
				ClownMDEmu_Callbacks callbacks = {NULL, CartridgeReadCallback, CartridgeWrittenCallback, ColourUpdatedCallback, ScanlineRenderedCallback, ReadInputCallback, FMAudioCallback, PSGAudioCallback};

				// If the user passed the path to the software on the command line, then load it here, automatically.
				if (argc > 1)
					OpenSoftware(argv[1], &callbacks);

				// Manages whether the program exits or not.
				bool quit = false;

				// Used for deciding when to pass inputs to the emulator.
				bool emulator_has_focus = false;

				// Used for tracking when to pop the emulation display out into its own little window.
				bool pop_out = false;

				bool emulator_paused = false;

				bool integer_screen_scaling = false;
				bool tall_double_resolution_mode = false;

				bool m68k_status = false;
				bool m68k_ram_viewer = false;
				bool z80_ram_viewer = false;
				bool plane_a_viewer = false;
				bool plane_b_viewer = false;
				bool vram_viewer = false;
				bool cram_viewer = false;
				bool dac_status = false;
				bool fm_status[6] = {false, false, false, false, false, false};
				bool psg_status = false;

				bool dear_imgui_demo_window = false;

				while (!quit)
				{
					const bool emulator_on = rom_buffer != NULL;
					const bool emulator_running = emulator_on && !emulator_paused
					#ifdef CLOWNMDEMUFRONTEND_REWINDING
						&& !(rewind_in_progress && state_rewind_remaining == 0)
					#endif
						;

				#ifdef CLOWNMDEMUFRONTEND_REWINDING
					// Handle rewinding.
					if (emulator_running)
					{
						// We maintain a ring buffer of emulator states:
						// when rewinding, we go backwards through this buffer,
						// and when not rewinding, we go forwards through it.
						size_t from_index, to_index;

						if (rewind_in_progress)
						{
							--state_rewind_remaining;

							from_index = to_index = state_rewind_index;

							if (from_index == 0)
								from_index = CC_COUNT_OF(state_rewind_buffer) - 1;
							else
								--from_index;

							state_rewind_index = from_index;
						}
						else
						{
							if (state_rewind_remaining < CC_COUNT_OF(state_rewind_buffer) - 1)
								++state_rewind_remaining;

							from_index = to_index = state_rewind_index;

							if (to_index == CC_COUNT_OF(state_rewind_buffer) - 1)
								to_index = 0;
							else
								++to_index;

							state_rewind_index = to_index;
						}

						SDL_memcpy(&state_rewind_buffer[to_index], &state_rewind_buffer[from_index], sizeof(state_rewind_buffer[0]));

						emulation_state = &state_rewind_buffer[to_index];
						ClownMDEmu_Parameters_Initialise(&clownmdemu, &clownmdemu_configuration, &clownmdemu_constant, &emulation_state->clownmdemu);
					}
				#endif

					// This loop processes events and manages the framerate.
					for (;;)
					{
						// 300 is the magic number that prevents these calculations from ever dipping into numbers smaller than 1
						// (technically it's only required by the NTSC framerate: PAL doesn't need it).
						#define MULTIPLIER 300

						static Uint32 next_time;
						const Uint32 current_time = SDL_GetTicks() * MULTIPLIER;

						int timeout = 0;

						if (!SDL_TICKS_PASSED(current_time, next_time))
							timeout = (next_time - current_time) / MULTIPLIER;
						else if (SDL_TICKS_PASSED(current_time, next_time + 100 * MULTIPLIER)) // If we're way past our deadline, then resync to the current tick instead of fast-forwarding
							next_time = current_time;

						// Obtain an event
						SDL_Event event;
						if (!SDL_WaitEventTimeout(&event, timeout)) // If the timeout has expired and there are no more events, exit this loop
						{
							// Calculate when the next frame will be
							Uint32 delta;

							switch (clownmdemu_configuration.general.tv_standard)
							{
								default:
								case CLOWNMDEMU_TV_STANDARD_NTSC:
									// Run at roughly 59.94FPS (60 divided by 1.001)
									delta = CLOWNMDEMU_DIVIDE_BY_NTSC_FRAMERATE(1000ul * MULTIPLIER);
									break;

								case CLOWNMDEMU_TV_STANDARD_PAL:
									// Run at 50FPS
									delta = CLOWNMDEMU_DIVIDE_BY_PAL_FRAMERATE(1000ul * MULTIPLIER);
									break;
							}

							next_time += delta >> (fast_forward_in_progress ? 2 : 0);

							break;
						}

						#undef MULTIPLIER

						ImGui_ImplSDL2_ProcessEvent(&event);

						// Process the event
						switch (event.type)
						{
							case SDL_QUIT:
								quit = true;
								break;

							case SDL_KEYDOWN:
								// Ignore repeated key inputs caused by holding the key down
								if (event.key.repeat)
									break;

								// Special hotkeys that can be used even when not focussed on the emulation window.
								switch (event.key.keysym.sym)
								{
									case SDLK_ESCAPE:
										// Exit fullscreen
										if (fullscreen)
										{
											fullscreen = false;
											SetFullscreen(fullscreen);
										}

										break;

									case SDLK_PAUSE:
										emulator_paused = !emulator_paused;
										break;

									case SDLK_F11:
										// Toggle fullscreen
										fullscreen = !fullscreen;
										SetFullscreen(fullscreen);
										break;

									default:
										break;
								}

								// Don't use inputs that are for Dear ImGui
								if (!emulator_on || !emulator_has_focus)
									break;

								switch (event.key.keysym.sym)
								{
									case SDLK_TAB:
										// Ignore CTRL+TAB (used by Dear ImGui for cycling between windows),
										// and ALT+TAB (used by the OS for cycling its windows).
										if (SDL_GetModState() != KMOD_NONE)
											break;

										// Soft-reset console
										ClownMDEmu_Reset(&clownmdemu, &callbacks);

										emulator_paused = false;

										break;

									case SDLK_F1:
										// Toggle which joypad the keyboard controls
										keyboard_input.bound_joypad ^= 1;
										break;

									case SDLK_F5:
										// Save save state
										quick_save_exists = true;
										quick_save_state = *emulation_state;
										break;

									case SDLK_F9:
										// Load save state
										if (quick_save_exists)
										{
											ApplySaveState(&quick_save_state);

											emulator_paused = false;
										}

										break;

									default:
										break;

								}
								// Fallthrough
							case SDL_KEYUP:
							{
								const bool pressed = event.key.state == SDL_PRESSED;

								switch (event.key.keysym.scancode)
								{
									#define DO_KEY(state, code) case code: keyboard_input.buttons[state] = pressed; break

									DO_KEY(CLOWNMDEMU_BUTTON_UP, SDL_SCANCODE_W);
									DO_KEY(CLOWNMDEMU_BUTTON_DOWN, SDL_SCANCODE_S);
									DO_KEY(CLOWNMDEMU_BUTTON_LEFT, SDL_SCANCODE_A);
									DO_KEY(CLOWNMDEMU_BUTTON_RIGHT, SDL_SCANCODE_D);
									DO_KEY(CLOWNMDEMU_BUTTON_A, SDL_SCANCODE_O);
									DO_KEY(CLOWNMDEMU_BUTTON_B, SDL_SCANCODE_P);
									DO_KEY(CLOWNMDEMU_BUTTON_C, SDL_SCANCODE_LEFTBRACKET);
									DO_KEY(CLOWNMDEMU_BUTTON_START, SDL_SCANCODE_RETURN);

									#undef DO_KEY

									case SDL_SCANCODE_SPACE:
										// Toggle fast-forward
										keyboard_input.fast_forward = pressed;
										UpdateFastForwardStatus();
										break;

									#ifdef CLOWNMDEMUFRONTEND_REWINDING
									case SDL_SCANCODE_R:
										keyboard_input.rewind = pressed;
										UpdateRewindStatus();
										break;
									#endif

									default:
										break;
								}

								break;
							}

							case SDL_CONTROLLERDEVICEADDED:
							{
								// Open the controller, and create an entry for it in the controller list.
								SDL_GameController *controller = SDL_GameControllerOpen(event.cdevice.which);

								if (controller == NULL)
								{
									PrintError("SDL_GameControllerOpen failed with the following message - '%s'", SDL_GetError());
								}
								else
								{
									const SDL_JoystickID joystick_instance_id = SDL_JoystickInstanceID(SDL_GameControllerGetJoystick(controller));

									if (joystick_instance_id < 0)
									{
										PrintError("SDL_JoystickInstanceID failed with the following message - '%s'", SDL_GetError());
									}
									else
									{
										ControllerInput *controller_input = (ControllerInput*)SDL_calloc(sizeof(ControllerInput), 1);

										if (controller_input == NULL)
										{
											PrintError("Could not allocate memory for the new ControllerInput struct");
										}
										else
										{
											controller_input->joystick_instance_id = joystick_instance_id;

											controller_input->next = controller_input_list_head;
											controller_input_list_head = controller_input;

											break;
										}
									}

									SDL_GameControllerClose(controller);
								}

								break;
							}

							case SDL_CONTROLLERDEVICEREMOVED:
							{
								// Close the controller, and remove it from the controller list.
								SDL_GameController *controller = SDL_GameControllerFromInstanceID(event.cdevice.which);

								if (controller == NULL)
								{
									PrintError("SDL_GameControllerFromInstanceID failed with the following message - '%s'", SDL_GetError());
								}
								else
								{
									SDL_GameControllerClose(controller);
								}

								for (ControllerInput **controller_input_pointer = &controller_input_list_head; ; controller_input_pointer = &(*controller_input_pointer)->next)
								{
									if ((*controller_input_pointer) == NULL)
									{
										PrintError("Received an SDL_CONTROLLERDEVICEREMOVED event for an unrecognised controller");
										break;
									}

									ControllerInput *controller_input = *controller_input_pointer;

									if (controller_input->joystick_instance_id == event.cdevice.which)
									{
										*controller_input_pointer = controller_input->next;
										SDL_free(controller_input);
										break;
									}
								}

								break;
							}

							case SDL_CONTROLLERBUTTONDOWN:
								if (event.cbutton.button == SDL_CONTROLLER_BUTTON_RIGHTSTICK)
								{
									// Toggle Dear ImGui gamepad controls.
									io.ConfigFlags ^= ImGuiConfigFlags_NavEnableGamepad;
								}

								// Don't use inputs that are for Dear ImGui
								if (!emulator_on || (io.ConfigFlags & ImGuiConfigFlags_NavEnableGamepad) != 0)
									break;

								// Fallthrough
							case SDL_CONTROLLERBUTTONUP:
							{
								const bool pressed = event.cbutton.state == SDL_PRESSED;

								// Look for the controller that this event belongs to.
								for (ControllerInput *controller_input = controller_input_list_head; ; controller_input = controller_input->next)
								{
									// If we've reached the end of the list, then somehow we've received an event for a controller that we haven't registered.
									if (controller_input == NULL)
									{
										PrintError("Received an SDL_CONTROLLERBUTTONDOWN/SDL_CONTROLLERBUTTONUP event for an unrecognised controller");
										break;
									}

									// Check if the current controller is the one that matches this event.
									if (controller_input->joystick_instance_id == event.cbutton.which)
									{
										switch (event.cbutton.button)
										{
											#define DO_BUTTON(state, code) case code: controller_input->input.buttons[state] = pressed; break

											DO_BUTTON(CLOWNMDEMU_BUTTON_A, SDL_CONTROLLER_BUTTON_X);
											DO_BUTTON(CLOWNMDEMU_BUTTON_B, SDL_CONTROLLER_BUTTON_Y);
											DO_BUTTON(CLOWNMDEMU_BUTTON_C, SDL_CONTROLLER_BUTTON_B);
											DO_BUTTON(CLOWNMDEMU_BUTTON_B, SDL_CONTROLLER_BUTTON_A);
											DO_BUTTON(CLOWNMDEMU_BUTTON_START, SDL_CONTROLLER_BUTTON_START);

											#undef DO_BUTTON

											case SDL_CONTROLLER_BUTTON_DPAD_UP:
											case SDL_CONTROLLER_BUTTON_DPAD_DOWN:
											case SDL_CONTROLLER_BUTTON_DPAD_LEFT:
											case SDL_CONTROLLER_BUTTON_DPAD_RIGHT:
											{
												unsigned int direction;
												unsigned int button;

												switch (event.cbutton.button)
												{
													default:
													case SDL_CONTROLLER_BUTTON_DPAD_UP:
														direction = 0;
														button = CLOWNMDEMU_BUTTON_UP;
														break;

													case SDL_CONTROLLER_BUTTON_DPAD_DOWN:
														direction = 1;
														button = CLOWNMDEMU_BUTTON_DOWN;
														break;

													case SDL_CONTROLLER_BUTTON_DPAD_LEFT:
														direction = 2;
														button = CLOWNMDEMU_BUTTON_LEFT;
														break;

													case SDL_CONTROLLER_BUTTON_DPAD_RIGHT:
														direction = 3;
														button = CLOWNMDEMU_BUTTON_RIGHT;
														break;
												}

												controller_input->dpad[direction] = pressed;

												// Combine D-pad and left stick values into final joypad D-pad inputs.
												controller_input->input.buttons[button] = controller_input->left_stick[direction] || controller_input->dpad[direction];

												break;
											}

											case SDL_CONTROLLER_BUTTON_BACK:
												// Toggle which joypad the controller is bound to.
												if (pressed)
													controller_input->input.bound_joypad ^= 1;

												break;

											#ifdef CLOWNMDEMUFRONTEND_REWINDING
											case SDL_CONTROLLER_BUTTON_LEFTSHOULDER:
												controller_input->input.rewind = pressed;
												UpdateRewindStatus();
												break;
											#endif

											case SDL_CONTROLLER_BUTTON_RIGHTSHOULDER:
												controller_input->input.fast_forward = pressed;
												UpdateFastForwardStatus();
												break;

											default:
												break;
										}

										break;
									}
								}

								break;
							}

							case SDL_CONTROLLERAXISMOTION:
								// Look for the controller that this event belongs to.
								for (ControllerInput *controller_input = controller_input_list_head; ; controller_input = controller_input->next)
								{
									// If we've reached the end of the list, then somehow we've received an event for a controller that we haven't registered.
									if (controller_input == NULL)
									{
										PrintError("Received an SDL_CONTROLLERAXISMOTION event for an unrecognised controller");
										break;
									}

									// Check if the current controller is the one that matches this event.
									if (controller_input->joystick_instance_id == event.caxis.which)
									{
										switch (event.caxis.axis)
										{
											case SDL_CONTROLLER_AXIS_LEFTX:
											case SDL_CONTROLLER_AXIS_LEFTY:
											{
												if (event.caxis.axis == SDL_CONTROLLER_AXIS_LEFTX)
													controller_input->left_stick_x = event.caxis.value;
												else //if (event.caxis.axis == SDL_CONTROLLER_AXIS_LEFTY)
													controller_input->left_stick_y = event.caxis.value;

												// Now that we have the left stick's X and Y values, let's do some trigonometry to figure out which direction(s) it's pointing in.

												// To start with, let's treat the X and Y values as a vector, and turn it into a unit vector.
												const float magnitude = SDL_sqrtf((float)(controller_input->left_stick_x * controller_input->left_stick_x + controller_input->left_stick_y * controller_input->left_stick_y));

												const float left_stick_x_unit = controller_input->left_stick_x / magnitude;
												const float left_stick_y_unit = controller_input->left_stick_y / magnitude;

												// Now that we have the stick's direction in the form of a unit vector,
												// we can create a dot product of it with another directional unit vector
												// to determine the angle between them.
												for (unsigned int i = 0; i < 4; ++i)
												{
													// Apply a deadzone.
													if (magnitude < 32768.0f / 4.0f)
													{
														controller_input->left_stick[i] = false;
													}
													else
													{
														// This is a list of directions expressed as unit vectors.
														static const float directions[4][2] = {
															{ 0.0f, -1.0f}, // Up
															{ 0.0f,  1.0f}, // Down
															{-1.0f,  0.0f}, // Left
															{ 1.0f,  0.0f}  // Right
														};

														// Perform dot product of stick's direction vector with other direction vector.
														const float delta_angle = SDL_acosf(left_stick_x_unit * directions[i][0] + left_stick_y_unit * directions[i][1]);

														// If the stick is within 67.5 degrees of the specified direction, then this will be true.
														controller_input->left_stick[i] = (delta_angle < (360.0f * 3.0f / 8.0f / 2.0f) * ((float)CC_PI / 180.0f)); // Half of 3/8 of 360 degrees converted to radians
													}

													static const unsigned int buttons[4] = {
														CLOWNMDEMU_BUTTON_UP,
														CLOWNMDEMU_BUTTON_DOWN,
														CLOWNMDEMU_BUTTON_LEFT,
														CLOWNMDEMU_BUTTON_RIGHT
													};

													// Combine D-pad and left stick values into final joypad D-pad inputs.
													controller_input->input.buttons[buttons[i]] = controller_input->left_stick[i] || controller_input->dpad[i];
												}

												break;
											}

											default:
												break;
										}

										break;
									}
								}

								break;

							case SDL_DROPFILE:
								OpenSoftware(event.drop.file, &callbacks);
								SDL_free(event.drop.file);

								emulator_paused = false;

								break;

							default:
								break;
						}
					}

					// Handle dynamic DPI support
					const float new_dpi = GetNewDPIScale();
					if (dpi_scale != new_dpi) // 96 DPI appears to be the "normal" DPI
					{
						style.ScaleAllSizes(new_dpi / dpi_scale);

						dpi_scale = new_dpi;

						ReloadFonts(CalculateFontSize());
					}

					if (emulator_running)
					{
						// Reset the audio buffers so that they can be mixed into.
						if (audio_device != 0)
							Mixer_Begin(&mixer);

						// Lock the texture so that we can write to its pixels later
						if (SDL_LockTexture(framebuffer_texture, NULL, (void**)&framebuffer_texture_pixels, &framebuffer_texture_pitch) < 0)
							framebuffer_texture_pixels = NULL;

						framebuffer_texture_pitch /= sizeof(Uint32);

						// Run the emulator for a frame
						ClownMDEmu_Iterate(&clownmdemu, &callbacks);

						// Unlock the texture so that we can draw it
						SDL_UnlockTexture(framebuffer_texture);

						// Resample, mix, and output the audio for this frame.
						// If there's a lot of audio queued, then don't queue any more.
						if (audio_device != 0 && SDL_GetQueuedAudioSize(audio_device) < audio_device_buffer_size * 2)
							Mixer_End(&mixer, AudioPushCallback, NULL);
					}

					// Start the Dear ImGui frame
					ImGui_ImplSDLRenderer_NewFrame();
					ImGui_ImplSDL2_NewFrame();
					ImGui::NewFrame();

					if (dear_imgui_demo_window)
						ImGui::ShowDemoWindow(&dear_imgui_demo_window);

					const ImGuiViewport *viewport = ImGui::GetMainViewport();

					// Maximise the window if needed.
					if (!pop_out)
					{
						ImGui::SetNextWindowPos(viewport->WorkPos);
						ImGui::SetNextWindowSize(viewport->WorkSize);
					}

					// Prevent the window from getting too small or we'll get division by zero errors later on.
					ImGui::SetNextWindowSizeConstraints(ImVec2(100.0f * dpi_scale, 100.0f * dpi_scale), ImVec2(FLT_MAX, FLT_MAX)); // Width > 100, Height > 100

					const bool show_menu_bar = !fullscreen
					                        || pop_out
					                        || m68k_status
					                        || m68k_ram_viewer
					                        || z80_ram_viewer
					                        || plane_a_viewer
					                        || plane_b_viewer
					                        || vram_viewer
					                        || cram_viewer
					                        || dac_status
					                        || fm_status[0]
					                        || fm_status[1]
					                        || fm_status[2]
					                        || fm_status[3]
					                        || fm_status[4]
					                        || fm_status[5]
					                        || psg_status
					                        || (io.ConfigFlags & ImGuiConfigFlags_NavEnableGamepad) != 0;

					// Hide mouse when the user just wants a fullscreen display window
					if (!show_menu_bar)
						ImGui::SetMouseCursor(ImGuiMouseCursor_None);

					// We don't want the emulator window overlapping the others while it's maximised.
					ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoBringToFrontOnFocus;

					// Hide as much of the window as possible when maximised.
					if (!pop_out)
						window_flags |= ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoBackground;

					// Hide the menu bar when maximised in fullscreen.
					if (show_menu_bar)
						window_flags |= ImGuiWindowFlags_MenuBar;

					// Tweak the style so that the display fill the window.
					ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
					const bool not_collapsed = ImGui::Begin("Display", NULL, window_flags);
					ImGui::PopStyleVar();

					if (not_collapsed)
					{
						if (show_menu_bar && ImGui::BeginMenuBar())
						{
							if (ImGui::BeginMenu("Console"))
							{
								if (ImGui::MenuItem("Open Software..."))
								{
									const char *rom_path = OpenFileDialog("Select Mega Drive Software", NULL, 0, NULL, NULL, 0);

									if (rom_path != NULL)
									{
										OpenSoftware(rom_path, &callbacks);

										emulator_paused = false;
									}
								}

								if (ImGui::MenuItem("Close Software", NULL, false, emulator_on))
								{
									SDL_free(rom_buffer);
									rom_buffer = NULL;
									rom_buffer_size = 0;

									emulator_paused = false;
								}

								ImGui::Separator();

								ImGui::MenuItem("Pause", "Pause", &emulator_paused, emulator_on);

								if (ImGui::MenuItem("Reset", "Tab", false, emulator_on))
								{
									// Soft-reset console
									ClownMDEmu_Reset(&clownmdemu, &callbacks);

									emulator_paused = false;
								}

								ImGui::Separator();

								if (ImGui::BeginMenu("Settings"))
								{
									ImGui::MenuItem("TV Standard", NULL, false, false);

									if (ImGui::MenuItem("NTSC (59.94Hz)", NULL, clownmdemu_configuration.general.tv_standard == CLOWNMDEMU_TV_STANDARD_NTSC))
									{
										if (clownmdemu_configuration.general.tv_standard != CLOWNMDEMU_TV_STANDARD_NTSC)
										{
											clownmdemu_configuration.general.tv_standard = CLOWNMDEMU_TV_STANDARD_NTSC;
											SetAudioPALMode(false);
										}
									}

									if (ImGui::MenuItem("PAL (50Hz)", NULL, clownmdemu_configuration.general.tv_standard == CLOWNMDEMU_TV_STANDARD_PAL))
									{
										if (clownmdemu_configuration.general.tv_standard != CLOWNMDEMU_TV_STANDARD_PAL)
										{
											clownmdemu_configuration.general.tv_standard = CLOWNMDEMU_TV_STANDARD_PAL;
											SetAudioPALMode(true);
										}
									}

									ImGui::Separator();

									ImGui::MenuItem("Region", NULL, false, false);

									if (ImGui::MenuItem("Domestic (Japan)", NULL, clownmdemu_configuration.general.region == CLOWNMDEMU_REGION_DOMESTIC))
										clownmdemu_configuration.general.region = CLOWNMDEMU_REGION_DOMESTIC;

									if (ImGui::MenuItem("Overseas (Elsewhere)", NULL, clownmdemu_configuration.general.region == CLOWNMDEMU_REGION_OVERSEAS))
										clownmdemu_configuration.general.region = CLOWNMDEMU_REGION_OVERSEAS;

									ImGui::EndMenu();
								}
								ImGui::EndMenu();
							}

							if (ImGui::BeginMenu("Save States"))
							{
								if (ImGui::MenuItem("Quick Save", "F5", false, emulator_on))
								{
									quick_save_exists = true;
									quick_save_state = *emulation_state;
								}

								if (ImGui::MenuItem("Quick Load", "F9", false, emulator_on && quick_save_exists))
								{
									ApplySaveState(&quick_save_state);

									emulator_paused = false;
								}

								ImGui::Separator();

								static const char save_state_magic[8] = "CMDEFSS"; // Clownacy Mega Drive Emulator Frontend Save State

								if (ImGui::MenuItem("Save To File...", NULL, false, emulator_on))
								{
									// Obtain a filename and path from the user.
									const char *save_state_path = SaveFileDialog("Create Save State", NULL, 0, NULL, NULL);

									if (save_state_path != NULL)
									{
										// Save the current state to the specified file.
										SDL_RWops *file = SDL_RWFromFile(save_state_path, "wb");

										if (file == NULL)
										{
											PrintError("Could not open save state file for writing");
											SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Error", "Could not create save state file.", window);
										}
										else
										{
											if (SDL_RWwrite(file, save_state_magic, sizeof(save_state_magic), 1) != 1)
											{
												PrintError("Could not write save state file");
												SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Error", "Could not create save state file.", window);
											}
											else
											{
												if (SDL_RWwrite(file, emulation_state, sizeof(*emulation_state), 1) != 1)
												{
													PrintError("Could not write save state file");
													SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Error", "Could not create save state file.", window);
												}
											}

											SDL_RWclose(file);
										}
									}
								}

								if (ImGui::MenuItem("Load From File...", NULL, false, emulator_on))
								{
									// Obtain a filename and path from the user.
									const char *save_state_path = OpenFileDialog("Load Save State", NULL, 0, NULL, NULL, 0);

									if (save_state_path != NULL)
									{
										// Load the state from the specified file.
										SDL_RWops *file = SDL_RWFromFile(save_state_path, "rb");

										if (file == NULL)
										{
											PrintError("Could not open save state file for reading");
											SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Error", "Could not load save state file.", window);
										}
										else
										{
											if (SDL_RWsize(file) != sizeof(save_state_magic) + sizeof(EmulationState))
											{
												PrintError("Invalid save state size");
												SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Error", "The file was not a valid save state.", window);
											}
											else
											{
												char magic_buffer[sizeof(save_state_magic)];

												if (SDL_RWread(file, magic_buffer, sizeof(magic_buffer), 1) != 1)
												{
													PrintError("Could not read save state file");
													SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Error", "Could not load save state file.", window);
												}
												else
												{
													if (SDL_memcmp(magic_buffer, save_state_magic, sizeof(save_state_magic)) != 0)
													{
														PrintError("Invalid save state magic");
														SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Error", "The file was not a valid save state.", window);
													}
													else
													{
														EmulationState *save_state;

														save_state = (EmulationState*)SDL_malloc(sizeof(EmulationState));

														if (save_state != NULL && SDL_RWread(file, save_state, sizeof(EmulationState), 1) != 1)
														{
															PrintError("Could not read save state file");
															SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Error", "Could not load save state file.", window);
														}
														else
														{
															ApplySaveState(save_state);

															emulator_paused = false;
														}

														SDL_free(save_state);
													}
												}
											}

											SDL_RWclose(file);
										}
									}
								}

								ImGui::EndMenu();
							}

							if (ImGui::BeginMenu("Debugging"))
							{
								ImGui::MenuItem("68000 Status", NULL, &m68k_status);

								ImGui::MenuItem("68000 RAM Viewer", NULL, &m68k_ram_viewer);

								ImGui::MenuItem("Z80 RAM Viewer", NULL, &z80_ram_viewer);

								if (ImGui::BeginMenu("VDP"))
								{
									ImGui::MenuItem("Plane A Viewer", NULL, &plane_a_viewer);

									ImGui::MenuItem("Plane B Viewer", NULL, &plane_b_viewer);

									ImGui::MenuItem("VRAM Viewer", NULL, &vram_viewer);

									ImGui::MenuItem("CRAM Viewer", NULL, &cram_viewer);

									ImGui::Separator();

									if (ImGui::MenuItem("Disable Sprite Plane", NULL, clownmdemu_configuration.vdp.sprites_disabled))
										clownmdemu_configuration.vdp.sprites_disabled = !clownmdemu_configuration.vdp.sprites_disabled;

									if (ImGui::MenuItem("Disable Plane A", NULL, clownmdemu_configuration.vdp.planes_disabled[0]))
										clownmdemu_configuration.vdp.planes_disabled[0] = !clownmdemu_configuration.vdp.planes_disabled[0];

									if (ImGui::MenuItem("Disable Plane B", NULL, clownmdemu_configuration.vdp.planes_disabled[1]))
										clownmdemu_configuration.vdp.planes_disabled[1] = !clownmdemu_configuration.vdp.planes_disabled[1];

									ImGui::EndMenu();
								}

								if (ImGui::BeginMenu("FM"))
								{
									ImGui::MenuItem("DAC Status", NULL, &dac_status);

									ImGui::MenuItem("Channel 1 Status", NULL, &fm_status[0]);
									ImGui::MenuItem("Channel 2 Status", NULL, &fm_status[1]);
									ImGui::MenuItem("Channel 3 Status", NULL, &fm_status[2]);
									ImGui::MenuItem("Channel 4 Status", NULL, &fm_status[3]);
									ImGui::MenuItem("Channel 5 Status", NULL, &fm_status[4]);
									ImGui::MenuItem("Channel 6 Status", NULL, &fm_status[5]);

									ImGui::EndMenu();
								}

								ImGui::MenuItem("PSG Status", NULL, &psg_status);

								ImGui::EndMenu();
							}

							if (ImGui::BeginMenu("Misc."))
							{
								if (ImGui::MenuItem("Fullscreen", "F11", &fullscreen))
									SetFullscreen(fullscreen);

								if (ImGui::MenuItem("V-Sync", NULL, &use_vsync))
									if (!fast_forward_in_progress)
										SDL_RenderSetVSync(renderer, use_vsync);

								if (ImGui::MenuItem("Integer Screen Scaling", NULL, &integer_screen_scaling) && integer_screen_scaling)
								{
									// Reclaim memory used by the upscaled framebuffer, since we won't be needing it anymore.
									SDL_DestroyTexture(framebuffer_texture_upscaled);
									framebuffer_texture_upscaled = NULL;
								}

								ImGui::MenuItem("Tall Double Resolution Mode", NULL, &tall_double_resolution_mode);

								if (ImGui::MenuItem("Low-Pass Filter", NULL, &low_pass_filter))
									Mixer_State_Initialise(&mixer_state, audio_device_sample_rate, pal_mode, low_pass_filter);

								ImGui::MenuItem("Pop-Out Display Window", NULL, &pop_out);

							#ifndef NDEBUG
								ImGui::Separator();

								ImGui::MenuItem("Show Dear ImGui Demo Window", NULL, &dear_imgui_demo_window);
							#endif

								ImGui::Separator();

								if (ImGui::MenuItem("Exit"))
									quit = true;

								ImGui::EndMenu();
							}

							ImGui::EndMenuBar();
						}

						// We need this block of logic to be outside of the below condition so that the emulator
						// has keyboard focus on startup without needing the window to be clicked first.
						// Weird behaviour - I know.
						const ImVec2 size_of_display_region = ImGui::GetContentRegionAvail();

						// Create an invisible button which detects when input is intended for the emulator.
						// We do this cursor stuff so that the framebuffer is drawn over the button.
						const ImVec2 cursor = ImGui::GetCursorPos();
						ImGui::InvisibleButton("Magical emulator focus detector", size_of_display_region);

						if (emulator_on)
						{
							emulator_has_focus = ImGui::IsItemFocused();
							ImGui::SetCursorPos(cursor);

							SDL_Texture *selected_framebuffer_texture = framebuffer_texture;

							const unsigned int work_width = (unsigned int)size_of_display_region.x;
							const unsigned int work_height = (unsigned int)size_of_display_region.y;

							unsigned int destination_width;
							unsigned int destination_height;

							switch (current_screen_height)
							{
								default:
									assert(false);
									// Fallthrough
								case 224:
									destination_width = 320;
									destination_height = 224;
									break;

								case 240:
									destination_width = 320;
									destination_height = 240;
									break;

								case 448:
									destination_width = tall_double_resolution_mode ? 320 : 640;
									destination_height = 448;
									break;

								case 480:
									destination_width = tall_double_resolution_mode ? 320 : 640;
									destination_height = 480;
									break;
							}

							unsigned int destination_width_scaled;
							unsigned int destination_height_scaled;

							ImVec2 uv1 = {(float)current_screen_width / (float)FRAMEBUFFER_WIDTH, (float)current_screen_height / (float)FRAMEBUFFER_HEIGHT};

							if (integer_screen_scaling)
							{
								// Round down to the nearest multiple of 'destination_width' and 'destination_height'.
								const unsigned int framebuffer_upscale_factor = CC_MAX(1, CC_MIN(work_width / destination_width, work_height / destination_height));

								destination_width_scaled = destination_width * framebuffer_upscale_factor;
								destination_height_scaled = destination_height * framebuffer_upscale_factor;
							}
							else
							{
								// Round to the nearest multiple of 'destination_width' and 'destination_height'.
								const unsigned int framebuffer_upscale_factor = CC_MAX(1, CC_MIN(CC_DIVIDE_ROUND(work_width, destination_width), CC_DIVIDE_ROUND(work_height, destination_height)));

								RecreateUpscaledFramebuffer(work_width, work_height);

								// Correct the aspect ratio of the rendered frame.
								// (256x224 and 320x240 should be the same width, but 320x224 and 320x240 should be different heights - this matches the behaviour of a real Mega Drive).
								if (work_width > work_height * destination_width / destination_height)
								{
									destination_width_scaled = work_height * destination_width / destination_height;
									destination_height_scaled = work_height;
								}
								else
								{
									destination_width_scaled = work_width;
									destination_height_scaled = work_width * destination_height / destination_width;
								}

								// Avoid blurring if...
								// 1. The upscale texture failed to be created.
								// 2. Blurring is unnecessary because the texture will be upscaled by an integer multiple.
								if (framebuffer_texture_upscaled != NULL && (destination_width_scaled % destination_width != 0 || destination_height_scaled % destination_height != 0))
								{
									// Render the upscaled framebuffer to the screen.
									selected_framebuffer_texture = framebuffer_texture_upscaled;

									// Before we can do that though, we have to actually render the upscaled framebuffer.
									SDL_Rect framebuffer_rect;
									framebuffer_rect.x = 0;
									framebuffer_rect.y = 0;
									framebuffer_rect.w = current_screen_width;
									framebuffer_rect.h = current_screen_height;

									SDL_Rect upscaled_framebuffer_rect;
									upscaled_framebuffer_rect.x = 0;
									upscaled_framebuffer_rect.y = 0;
									upscaled_framebuffer_rect.w = destination_width * framebuffer_upscale_factor;
									upscaled_framebuffer_rect.h = destination_height * framebuffer_upscale_factor;

									// Render to the upscaled framebuffer.
									SDL_SetRenderTarget(renderer, framebuffer_texture_upscaled);

									// Render.
									SDL_RenderCopy(renderer, framebuffer_texture, &framebuffer_rect, &upscaled_framebuffer_rect);

									// Switch back to actually rendering to the screen.
									SDL_SetRenderTarget(renderer, NULL);

									// Update the texture UV to suit the upscaled framebuffer.
									uv1.x = (float)upscaled_framebuffer_rect.w / (float)framebuffer_texture_upscaled_width;
									uv1.y = (float)upscaled_framebuffer_rect.h / (float)framebuffer_texture_upscaled_height;
								}
							}

							// Center the framebuffer in the available region.
							ImGui::SetCursorPosX((float)((int)ImGui::GetCursorPosX() + ((int)size_of_display_region.x - destination_width_scaled) / 2));
							ImGui::SetCursorPosY((float)((int)ImGui::GetCursorPosY() + ((int)size_of_display_region.y - destination_height_scaled) / 2));

							// Draw the upscaled framebuffer in the window.
							ImGui::Image(selected_framebuffer_texture, ImVec2((float)destination_width_scaled, (float)destination_height_scaled), ImVec2(0, 0), uv1);
						}
					}

					ImGui::End();

					if (m68k_status)
						Debug_M68k(&m68k_status, &clownmdemu.state->m68k, monospace_font);

					if (m68k_ram_viewer)
						Debug_Memory(&m68k_ram_viewer, monospace_font, "68000 RAM Viewer", clownmdemu.state->m68k_ram, CC_COUNT_OF(clownmdemu.state->m68k_ram));

					if (z80_ram_viewer)
						Debug_Memory(&z80_ram_viewer, monospace_font, "Z80 RAM Viewer", clownmdemu.state->z80_ram, CC_COUNT_OF(clownmdemu.state->z80_ram));

					const Debug_VDP_Data debug_vdp_data = {emulation_state->colours, renderer, dpi_scale};

					if (plane_a_viewer)
						Debug_PlaneA(&plane_a_viewer, &clownmdemu, &debug_vdp_data);

					if (plane_b_viewer)
						Debug_PlaneB(&plane_b_viewer, &clownmdemu, &debug_vdp_data);

					if (vram_viewer)
						Debug_VRAM(&vram_viewer, &clownmdemu, &debug_vdp_data);

					if (cram_viewer)
						Debug_CRAM(&cram_viewer, &clownmdemu, &debug_vdp_data, monospace_font);

					if (dac_status)
						Debug_DAC_Channel(&dac_status, &clownmdemu, monospace_font);

					for (unsigned int i = 0; i < 6; ++i)
						if (fm_status[i])
							Debug_FM_Channel(&fm_status[i], &clownmdemu, monospace_font, i);

					if (psg_status)
						Debug_PSG(&psg_status, &clownmdemu, monospace_font);

					SDL_RenderClear(renderer);

					// Render Dear ImGui.
					ImGui::Render();
					ImGui_ImplSDLRenderer_RenderDrawData(ImGui::GetDrawData());

					// Finally display the rendered frame to the user.
					SDL_RenderPresent(renderer);
				}

				SDL_free(rom_buffer);

				DeinitialiseAudio();

				ImGui_ImplSDLRenderer_Shutdown();
				ImGui_ImplSDL2_Shutdown();
				ImGui::DestroyContext();

				SDL_DestroyTexture(framebuffer_texture_upscaled);

				DeinitialiseFramebuffer();
			}

			DeinitialiseVideo();
		}

		SDL_Quit();
	}

	return 0;
}
