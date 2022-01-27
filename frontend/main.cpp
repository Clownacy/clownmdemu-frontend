#include <stdarg.h>
#include <stddef.h>

#include "SDL.h"

#include "../clownmdemu.h"

#define CLOWNRESAMPLER_IMPLEMENTATION
#define CLOWNRESAMPLER_API static
#define CLOWNRESAMPLER_ASSERT SDL_assert
#define CLOWNRESAMPLER_FABS SDL_fabs
#define CLOWNRESAMPLER_SIN SDL_sin
#define CLOWNRESAMPLER_MEMSET SDL_memset
#define CLOWNRESAMPLER_MEMMOVE SDL_memmove
#include "clownresampler/clownresampler.h"

#include "tinyfiledialogs.h"

#include "imgui/imgui.h"
#include "imgui/imgui_impl_sdl.h"
#include "imgui/imgui_impl_sdlrenderer.h"

#if !SDL_VERSION_ATLEAST(2,0,17)
#error SDL 2.0.17+ is required because of SDL_RenderGeometry() function!
#endif

#include "imgui/karla_regular.h"

typedef struct Input
{
	unsigned int bound_joypad;
	bool buttons[CLOWNMDEMU_BUTTON_MAX];
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

typedef struct SaveState
{
	ClownMDEmu_State state;
	Uint32 colours[3 * 4 * 16];
} SaveState;

static Input keyboard_input;

static ControllerInput *controller_input_list_head;

static ClownMDEmu_State clownmdemu_state;

static bool quick_save_exists = false;
static SaveState quick_save_state;

static unsigned char *rom_buffer;
static size_t rom_buffer_size;

static ClownMDEmu_Region region = CLOWNMDEMU_REGION_OVERSEAS;
static ClownMDEmu_TVStandard tv_standard = CLOWNMDEMU_TV_STANDARD_NTSC;

static void PrintErrorInternal(const char *format, va_list args)
{
	SDL_LogMessageV(SDL_LOG_CATEGORY_ERROR, SDL_LOG_PRIORITY_ERROR, format, args);
}

static void PrintError(const char *format, ...)
{
	va_list args;
	va_start(args, format);
	PrintErrorInternal(format, args);
	va_end(args);
}

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
#define FRAMEBUFFER_HEIGHT (240*2)

static SDL_Window *window;
static SDL_Renderer *renderer;
static SDL_Texture *framebuffer_texture;
static Uint32 *framebuffer_texture_pixels;
static int framebuffer_texture_pitch;
static SDL_Texture *framebuffer_texture_upscaled;
static unsigned int framebuffer_upscale_factor;

static Uint32 colours[3 * 4 * 16];

static unsigned int current_screen_width;
static unsigned int current_screen_height;

static bool use_vsync;
static bool fullscreen;

static bool InitVideo(void)
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
			// Create renderer
			renderer = SDL_CreateRenderer(window, -1, 0);

			if (renderer == NULL)
			{
				PrintError("SDL_CreateRenderer failed with the following message - '%s'", SDL_GetError());
			}
			else
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

					// Lock the texture so that we can write to its pixels later
					if (SDL_LockTexture(framebuffer_texture, NULL, (void**)&framebuffer_texture_pixels, &framebuffer_texture_pitch) < 0)
						framebuffer_texture_pixels = NULL;

					framebuffer_texture_pitch /= sizeof(Uint32);

					return true;
				}

				SDL_DestroyRenderer(renderer);
			}

			SDL_DestroyWindow(window);
		}

		SDL_QuitSubSystem(SDL_INIT_VIDEO);
	}

	return false;
}

static void DeinitVideo(void)
{
	SDL_DestroyTexture(framebuffer_texture);
	SDL_DestroyRenderer(renderer);
	SDL_DestroyWindow(window);
	SDL_QuitSubSystem(SDL_INIT_VIDEO);
}

static void SetFullscreen(bool enabled)
{
	SDL_SetWindowFullscreen(window, enabled ? SDL_WINDOW_FULLSCREEN_DESKTOP : 0);

	// Prevent Dear ImGui from making the cursor visible if needed
	ImGuiIO &io = ImGui::GetIO();

	if (enabled)
		io.ConfigFlags |= ImGuiConfigFlags_NoMouseCursorChange;
	else
		io.ConfigFlags &= ~ImGuiConfigFlags_NoMouseCursorChange;

	SDL_ShowCursor(enabled ? SDL_DISABLE : SDL_ENABLE);
}

void RecreateUpscaledFramebuffer(int destination_width, int destination_height)
{
	const unsigned int source_width = current_screen_width;
	const unsigned int source_height = current_screen_height;

	// Round to the nearest multiples of FRAMEBUFFER_WIDTH and FRAMEBUFFER_HEIGHT
	framebuffer_upscale_factor = CC_MAX(1, CC_MIN((destination_width + source_width / 2) / source_width, (destination_height + source_height / 2) / source_height));

	static unsigned int previous_framebuffer_upscale_factor = 0;

	if (framebuffer_upscale_factor != previous_framebuffer_upscale_factor)
	{
		previous_framebuffer_upscale_factor = framebuffer_upscale_factor;

		SDL_DestroyTexture(framebuffer_texture_upscaled); // It should be safe to pass NULL to this
		SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "linear");
		framebuffer_texture_upscaled = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_TARGET, FRAMEBUFFER_WIDTH * framebuffer_upscale_factor, FRAMEBUFFER_HEIGHT * framebuffer_upscale_factor);

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
static size_t audio_buffer_size;
static ClownResampler_HighLevel_State resampler;
static int native_audio_sample_rate;

static void SetAudioPALMode(bool enabled)
{
	const unsigned int pal_sample_rate = CLOWNMDEMU_MULTIPLY_BY_PAL_FRAMERATE(CLOWNMDEMU_DIVIDE_BY_PAL_FRAMERATE(CLOWNMDEMU_MASTER_CLOCK_PAL / 15 / 16));
	const unsigned int ntsc_sample_rate = CLOWNMDEMU_MULTIPLY_BY_NTSC_FRAMERATE(CLOWNMDEMU_DIVIDE_BY_NTSC_FRAMERATE(CLOWNMDEMU_MASTER_CLOCK_NTSC / 15 / 16));

	SDL_LockAudioDevice(audio_device);
	ClownResampler_HighLevel_Init(&resampler, 1, enabled ? pal_sample_rate : ntsc_sample_rate, native_audio_sample_rate);
	SDL_UnlockAudioDevice(audio_device);
}

static bool InitAudio(void)
{
	if (SDL_InitSubSystem(SDL_INIT_AUDIO) < 0)
	{
		PrintError("SDL_InitSubSystem(SDL_INIT_AUDIO) failed with the following message - '%s'", SDL_GetError());
	}
	else
	{
		SDL_LogSetPriority(SDL_LOG_CATEGORY_ERROR, SDL_LOG_PRIORITY_ERROR);

		// Initialise audio backend
		SDL_AudioSpec want, have;

		SDL_zero(want);
		want.freq = 48000;
		want.format = AUDIO_S16;
		want.channels = 1;
		// We want a 25ms buffer (this value must be a power of two)
		want.samples = 1;
		while (want.samples < (want.freq * want.channels) / (1000 / 25))
			want.samples <<= 1;
		want.callback = NULL;

		audio_device = SDL_OpenAudioDevice(NULL, 0, &want, &have, SDL_AUDIO_ALLOW_FREQUENCY_CHANGE);

		if (audio_device == 0)
		{
			PrintError("SDL_OpenAudioDevice failed with the following message - '%s'", SDL_GetError());
		}
		else
		{
			audio_buffer_size = have.size;
			native_audio_sample_rate = have.freq;

			SetAudioPALMode(tv_standard == CLOWNMDEMU_TV_STANDARD_PAL);

			SDL_PauseAudioDevice(audio_device, 0);

			return true;
		}

		SDL_QuitSubSystem(SDL_INIT_AUDIO);
	}

	return false;
}

static void DeinitAudio(void)
{
	SDL_CloseAudioDevice(audio_device);
	SDL_QuitSubSystem(SDL_INIT_AUDIO);
}

static unsigned int CartridgeReadCallback(void *user_data, unsigned long address)
{
	(void)user_data;

	if (address >= rom_buffer_size)
		return 0;

	return rom_buffer[address];
}

static void CartridgeWrittenCallback(void *user_data, unsigned long address, unsigned int value)
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

static void ColourUpdatedCallback(void *user_data, unsigned int index, unsigned int colour)
{
	(void)user_data;

	// Decompose XBGR4444 into individual colour channels
	const Uint32 red = (colour >> 4 * 0) & 0xF;
	const Uint32 green = (colour >> 4 * 1) & 0xF;
	const Uint32 blue = (colour >> 4 * 2) & 0xF;

	// Reassemble into ARGB8888
	colours[index] = (blue << 4 * 0) | (blue << 4 * 1) | (green << 4 * 2) | (green << 4 * 3) | (red << 4 * 4) | (red << 4 * 5);
}

static void ScanlineRenderedCallback(void *user_data, unsigned int scanline, const unsigned char *pixels, unsigned int screen_width, unsigned int screen_height)
{
	(void)user_data;

	current_screen_width = screen_width;
	current_screen_height = screen_height;

	if (framebuffer_texture_pixels != NULL)
		for (unsigned int i = 0; i < screen_width; ++i)
			framebuffer_texture_pixels[scanline * framebuffer_texture_pitch + i] = colours[pixels[i]];
}

static cc_bool ReadInputCallback(void *user_data, unsigned int player_id, ClownMDEmu_Button button_id)
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

typedef struct ResamplerCallbackUserData
{
	ClownMDEmu_State *state;
	size_t samples_remaining;
} ResamplerCallbackUserData;

static size_t ResamplerCallback(void *user_data, short *buffer, size_t buffer_size)
{
	ResamplerCallbackUserData *resampler_callback_user_data = (ResamplerCallbackUserData*)user_data;

	const size_t samples_to_do = CC_MIN(resampler_callback_user_data->samples_remaining, buffer_size);

	SDL_memset(buffer, 0, samples_to_do * sizeof(*buffer));

	ClownMDEmu_GeneratePSGAudio(resampler_callback_user_data->state, buffer, samples_to_do);

	resampler_callback_user_data->samples_remaining -= samples_to_do;

	return samples_to_do;
}

static void PSGAudioCallback(void *user_data, size_t total_samples)
{
	short audio_buffer[0x400];

	ResamplerCallbackUserData resampler_callback_user_data;
	resampler_callback_user_data.state = (ClownMDEmu_State*)user_data;
	resampler_callback_user_data.samples_remaining = total_samples;

	size_t total_resampled_samples;
	while ((total_resampled_samples = ClownResampler_HighLevel_Resample(&resampler, audio_buffer, CC_COUNT_OF(audio_buffer), ResamplerCallback, &resampler_callback_user_data)) != 0)
		if (SDL_GetQueuedAudioSize(audio_device) < audio_buffer_size * 2)
			SDL_QueueAudio(audio_device, audio_buffer, (Uint32)total_resampled_samples * sizeof(*audio_buffer));
}

static void CreateSaveState(SaveState *save_state)
{
	save_state->state = clownmdemu_state;
	SDL_memcpy(save_state->colours, colours, sizeof(colours));
}

static void ApplySaveState(SaveState *save_state)
{
	clownmdemu_state = save_state->state;
	SDL_memcpy(colours, save_state->colours, sizeof(colours));

	// We don't want the save state to override the user's configurations, so reapply them now.
	ClownMDEmu_SetRegion(&clownmdemu_state, region);
	ClownMDEmu_SetTVStandard(&clownmdemu_state, tv_standard);
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

		ClownMDEmu_Init(&clownmdemu_state, region, tv_standard);
		ClownMDEmu_Reset(&clownmdemu_state, callbacks);
	}
}

int main(int argc, char **argv)
{
	// Initialise SDL2
	if (SDL_Init(SDL_INIT_EVENTS | SDL_INIT_GAMECONTROLLER) < 0)
	{
		PrintError("SDL_Init failed with the following message - '%s'", SDL_GetError());
		SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Fatal Error", "Unable to initialise SDL2. The program will now close.", NULL);
	}
	else
	{
		if (!InitVideo())
		{
			PrintError("InitVideo failed");
			SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Fatal Error", "Unable to initialise video subsystem. The program will now close.", NULL);
		}
		else
		{
			// Get DPI scale
			float dpi_scale = 1.0f;

			float ddpi;
			if (SDL_GetDisplayDPI(SDL_GetWindowDisplayIndex(window), &ddpi, NULL, NULL) == 0)
				dpi_scale = ddpi / 96.0f; // 96 DPI appears to be the "normal" DPI

			// Setup Dear ImGui context
			IMGUI_CHECKVERSION();
			ImGui::CreateContext();
			ImGuiIO &io = ImGui::GetIO();
			ImGuiStyle &style = ImGui::GetStyle();
			io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
			//io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls

			// Setup Dear ImGui style
			ImGui::StyleColorsDark();
			//ImGui::StyleColorsLight();
			//ImGui::StyleColorsClassic();

			// Apply DPI scale (also resize the window so that there's room for the menu bar).
			style.ScaleAllSizes(dpi_scale);
			const float font_size = SDL_floorf(15.0f * dpi_scale);
			const float menu_bar_size = font_size + style.FramePadding.y * 2; // An inlined ImGui::GetFrameHeight that actually works
			SDL_SetWindowSize(window, (int)(320 * 2 * dpi_scale), (int)(224 * 2 * dpi_scale + menu_bar_size));

			// We are now ready to show the window
			SDL_ShowWindow(window);

			// Setup Platform/Renderer backends
			ImGui_ImplSDL2_InitForSDLRenderer(window, renderer);
			ImGui_ImplSDLRenderer_Init(renderer);

			// Load Font
			ImFontConfig font_cfg = ImFontConfig();
			SDL_snprintf(font_cfg.Name, IM_ARRAYSIZE(font_cfg.Name), "Karla Regular, %dpx", (int)font_size);
			io.Fonts->AddFontFromMemoryCompressedTTF(karla_regular_compressed_data, karla_regular_compressed_size, font_size, &font_cfg);

			// Intiialise audio if we can (but it's okay if it fails).
			const bool initialised_audio = InitAudio();

			if (!initialised_audio)
			{
				PrintError("InitAudio failed"); // Allow program to continue if audio fails
				SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_WARNING, "Warning", "Unable to initialise audio subsystem: the program will not output audio!", window);
			}

			// This should be called before any other clownmdemu functions are called!
			ClownMDEmu_SetErrorCallback(PrintErrorInternal);

			// Construct our big list of callbacks for clownmdemu.
			const ClownMDEmu_Callbacks callbacks = {&clownmdemu_state, CartridgeReadCallback, CartridgeWrittenCallback, ColourUpdatedCallback, ScanlineRenderedCallback, ReadInputCallback, PSGAudioCallback};

			// If the user passed the path to the software on the command line, then load it here, automatically.
			if (argc > 1)
				OpenSoftware(argv[1], &callbacks);

			// Manages whether the program exits or not.
			bool quit = false;

			// Manages whether the emulator runs at a higher speed or not.
			bool fast_forward = false;

			while (!quit)
			{
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

						switch (tv_standard)
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

						next_time += delta >> (fast_forward ? 2 : 0);

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
							// Don't use inputs that Dear ImGui wants
							if (io.WantCaptureKeyboard)
								break;

							// Ignore repeated key inputs caused by holding the key down
							if (event.key.repeat)
								break;

							switch (event.key.keysym.sym)
							{
								case SDLK_TAB:
									// Ignore CTRL+TAB (used by Dear ImGui for cycling between windows)
									if ((SDL_GetModState() & KMOD_CTRL) != 0)
										break;

									// Soft-reset console
									ClownMDEmu_Reset(&clownmdemu_state, &callbacks);

									break;

								case SDLK_ESCAPE:
									// Exit fullscreen
									if (fullscreen)
									{
										fullscreen = false;
										SetFullscreen(fullscreen);
									}

									break;

								case SDLK_F1:
									// Toggle which joypad the keyboard controls
									keyboard_input.bound_joypad ^= 1;
									break;

								case SDLK_F5:
									// Save save state
									quick_save_exists = true;
									CreateSaveState(&quick_save_state);
									break;

								case SDLK_F9:
									// Load save state
									if (quick_save_exists)
										ApplySaveState(&quick_save_state);

									break;

								case SDLK_F11:
									// Toggle fullscreen
									fullscreen = !fullscreen;
									SetFullscreen(fullscreen);
									break;

								default:
									break;

							}
							// Fallthrough
						case SDL_KEYUP:
						{
							// Don't use inputs that Dear ImGui wants
							if (io.WantCaptureKeyboard)
								break;

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
									fast_forward = pressed;

									// Disable V-sync so that 60Hz displays aren't locked to 1x speed while fast-forwarding
									if (use_vsync)
										SDL_RenderSetVSync(renderer, !pressed);

									break;

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
													const float directions[4][2] = {
														{ 0.0f, -1.0f}, // Up
														{ 0.0f,  1.0f}, // Down
														{-1.0f,  0.0f}, // Left
														{ 1.0f,  0.0f}  // Right
													};

													// Perform dot product of stick's direction vector with other direction vector.
													const float delta_angle = SDL_acosf(left_stick_x_unit * directions[i][0] + left_stick_y_unit * directions[i][1]);

													// If the stick is within 67.5 degrees of the specified direction, then this will be true.
													controller_input->left_stick[i] = (delta_angle < (360.0f * 3.0f / 8.0f / 2.0f) * ((float)M_PI / 180.0f)); // Half of 3/8 of 360 degrees converted to radians
												}

												const unsigned int buttons[4] = {
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
							break;

						default:
							break;
					}
				}

				// Start the Dear ImGui frame
				ImGui_ImplSDLRenderer_NewFrame();
				ImGui_ImplSDL2_NewFrame();
				ImGui::NewFrame();

				// 1. Show the big demo window (Most of the sample code is in ImGui::ShowDemoWindow()! You can browse its code to learn more about Dear ImGui!).
				//ImGui::ShowDemoWindow();

				// Handle the Dear ImGui-powered menu bar.
				if (!fullscreen && ImGui::BeginMainMenuBar())
				{
					if (ImGui::BeginMenu("Console"))
					{
						if (ImGui::MenuItem("Open Software..."))
						{
							const char *rom_path = tinyfd_openFileDialog("Select Mega Drive software", NULL, 0, NULL, NULL, 0);

							if (rom_path != NULL)
								OpenSoftware(rom_path, &callbacks);
						}

						if (ImGui::MenuItem("Close Software", NULL, false, rom_buffer != NULL))
						{
							SDL_free(rom_buffer);
							rom_buffer = NULL;
							rom_buffer_size = 0;
						}

						ImGui::Separator();

						if (ImGui::MenuItem("Reset", "Tab"))
						{
							// Soft-reset console
							ClownMDEmu_Reset(&clownmdemu_state, &callbacks);
						}

						ImGui::Separator();

						if (ImGui::BeginMenu("Settings"))
						{
							ImGui::MenuItem("TV Standard", NULL, false, false);

							if (ImGui::MenuItem("NTSC (60Hz)", NULL, tv_standard == CLOWNMDEMU_TV_STANDARD_NTSC))
							{
								if (tv_standard != CLOWNMDEMU_TV_STANDARD_NTSC)
								{
									tv_standard = CLOWNMDEMU_TV_STANDARD_NTSC;
									ClownMDEmu_SetTVStandard(&clownmdemu_state, tv_standard);
									SetAudioPALMode(false);
								}
							}

							if (ImGui::MenuItem("PAL (50Hz)", NULL, tv_standard == CLOWNMDEMU_TV_STANDARD_PAL))
							{
								if (tv_standard != CLOWNMDEMU_TV_STANDARD_PAL)
								{
									tv_standard = CLOWNMDEMU_TV_STANDARD_PAL;
									ClownMDEmu_SetTVStandard(&clownmdemu_state, tv_standard);
									SetAudioPALMode(true);
								}
							}

							ImGui::Separator();

							ImGui::MenuItem("Region", NULL, false, false);

							if (ImGui::MenuItem("Domestic (Japan)", NULL, region == CLOWNMDEMU_REGION_DOMESTIC))
							{
								if (region != CLOWNMDEMU_REGION_DOMESTIC)
								{
									region = CLOWNMDEMU_REGION_DOMESTIC;
									ClownMDEmu_SetRegion(&clownmdemu_state, region);
								}
							}

							if (ImGui::MenuItem("Overseas (Elsewhere)", NULL, region == CLOWNMDEMU_REGION_OVERSEAS))
							{
								if (region != CLOWNMDEMU_REGION_OVERSEAS)
								{
									region = CLOWNMDEMU_REGION_OVERSEAS;
									ClownMDEmu_SetRegion(&clownmdemu_state, region);
								}
							}

							ImGui::EndMenu();
						}
						ImGui::EndMenu();
					}

					if (ImGui::BeginMenu("Save States"))
					{
						if (ImGui::MenuItem("Quick Save", "F5", false, rom_buffer != NULL))
						{
							quick_save_exists = true;
							CreateSaveState(&quick_save_state);
						}

						if (ImGui::MenuItem("Quick Load", "F9", false, rom_buffer != NULL && quick_save_exists))
							ApplySaveState(&quick_save_state);

						ImGui::Separator();

						if (ImGui::MenuItem("Save To File...", NULL, false, rom_buffer != NULL))
						{
							// Obtain a filename and path from the user.
							const char *save_state_path = tinyfd_saveFileDialog("Create Save State", NULL, 0, NULL, NULL);

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
									SaveState save_state;
									CreateSaveState(&save_state);

									if (SDL_RWwrite(file, &save_state, sizeof(save_state), 1) != 1)
									{
										PrintError("Could not write save state file");
										SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Error", "Could not create save state file.", window);
									}

									SDL_RWclose(file);
								}
							}
						}

						if (ImGui::MenuItem("Load From File...", NULL, false, rom_buffer != NULL))
						{
							// Obtain a filename and path from the user.
							const char *save_state_path = tinyfd_openFileDialog("Load Save State", NULL, 0, NULL, NULL, 0);

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
									SaveState save_state;

									if (SDL_RWread(file, &save_state, sizeof(save_state), 1) != 1)
									{
										PrintError("Could not read save state file");
										SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Error", "Could not load save state file.", window);
									}
									else
									{
										ApplySaveState(&save_state);
									}

									SDL_RWclose(file);
								}
							}
						}

						ImGui::EndMenu();
					}

					if (ImGui::BeginMenu("Misc."))
					{
						if (ImGui::MenuItem("V-Sync", NULL, use_vsync))
						{
							use_vsync = !use_vsync;

							if (!fast_forward)
								SDL_RenderSetVSync(renderer, use_vsync);
						}

						if (ImGui::MenuItem("Fullscreen", "F11", fullscreen))
						{
							fullscreen = !fullscreen;
							SetFullscreen(fullscreen);
						}

						ImGui::Separator();

						if (ImGui::MenuItem("Exit"))
							quit = true;

						ImGui::EndMenu();
					}

					ImGui::EndMainMenuBar();
				}

				if (rom_buffer != NULL)
				{
					// Lock the texture so that we can write to its pixels later
					if (SDL_LockTexture(framebuffer_texture, NULL, (void**)&framebuffer_texture_pixels, &framebuffer_texture_pitch) < 0)
						framebuffer_texture_pixels = NULL;

					framebuffer_texture_pitch /= sizeof(Uint32);

					// Run the emulator for a frame
					ClownMDEmu_Iterate(&clownmdemu_state, &callbacks);

					// Unlock the texture so that we can draw it
					SDL_UnlockTexture(framebuffer_texture);
				}

				SDL_RenderClear(renderer);

				if (rom_buffer != NULL)
				{
					// Draw the rendered frame to the screen
					SDL_Rect framebuffer_rect;
					framebuffer_rect.x = 0;
					framebuffer_rect.y = 0;
					framebuffer_rect.w = current_screen_width;
					framebuffer_rect.h = current_screen_height;

					// Correct the aspect ratio of the rendered frame
					// (256x224 and 320x240 should be the same width, but 320x224 and 320x240 should be different heights - this matches the behaviour of a real Mega Drive)
					const ImGuiViewport* viewport = ImGui::GetMainViewport();
					const unsigned int work_width = (unsigned int)viewport->WorkSize.x;
					const unsigned int work_height = (unsigned int)viewport->WorkSize.y;

					RecreateUpscaledFramebuffer(work_width, work_height);

					SDL_Rect destination_rect;

					if (work_width > work_height * 320 / current_screen_height)
					{
						destination_rect.w = work_height * 320 / current_screen_height;
						destination_rect.h = work_height;
					}
					else
					{
						destination_rect.w = work_width;
						destination_rect.h = work_width * current_screen_height / 320;
					}

					destination_rect.x = (unsigned int)viewport->WorkPos.x + (work_width - destination_rect.w) / 2;
					destination_rect.y = (unsigned int)viewport->WorkPos.y + (work_height - destination_rect.h) / 2;

					// Avoid blurring if...
					// 1. The upscale texture failed to be created
					// 2. Blurring is unnecessary because the texture will be upscaled by an integer multiple
					if (framebuffer_texture_upscaled == NULL || (destination_rect.w % current_screen_width == 0 && destination_rect.h % current_screen_height == 0))
					{
						// Render the framebuffer to the screen
						SDL_RenderCopy(renderer, framebuffer_texture, &framebuffer_rect, &destination_rect);
					}
					else
					{
						// Upscale the framebuffer
						SDL_Rect upscaled_framebuffer_rect;
						upscaled_framebuffer_rect.x = 0;
						upscaled_framebuffer_rect.y = 0;
						upscaled_framebuffer_rect.w = current_screen_width * framebuffer_upscale_factor;
						upscaled_framebuffer_rect.h = current_screen_height * framebuffer_upscale_factor;

						SDL_SetRenderTarget(renderer, framebuffer_texture_upscaled);
						SDL_RenderCopy(renderer, framebuffer_texture, &framebuffer_rect, &upscaled_framebuffer_rect);

						// Render the upscaled framebuffer to the screen
						SDL_SetRenderTarget(renderer, NULL);
						SDL_RenderCopy(renderer, framebuffer_texture_upscaled, &upscaled_framebuffer_rect, &destination_rect);
					}
				}

				// Render Dear ImGui.
				ImGui::Render();
				ImGui_ImplSDLRenderer_RenderDrawData(ImGui::GetDrawData());

				// Finally display the rendered frame to the user.
				SDL_RenderPresent(renderer);
			}

			// Dump the state to a file (this is a debug feature).
			SDL_RWops *state_file = SDL_RWFromFile("state.bin", "wb");

			if (state_file != NULL)
			{
				SDL_RWwrite(state_file, &clownmdemu_state, 1, sizeof(clownmdemu_state));

				SDL_RWclose(state_file);
			}

			ClownMDEmu_Deinit(&clownmdemu_state);

			SDL_free(rom_buffer);

			if (initialised_audio)
				DeinitAudio();

			ImGui_ImplSDLRenderer_Shutdown();
			ImGui_ImplSDL2_Shutdown();
			ImGui::DestroyContext();

			SDL_DestroyTexture(framebuffer_texture_upscaled);
			DeinitVideo();
		}

		SDL_Quit();
	}

	return 0;
}
