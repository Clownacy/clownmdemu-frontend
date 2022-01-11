#include <assert.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

#include "SDL.h"

#include "../clownmdemu.h"

//#define PAL

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

static Input keyboard_input;

static ControllerInput *controller_input_list_head;

static ClownMDEmu_State clownmdemu_state;
static ClownMDEmu_State clownmdemu_save_state;

static void PrintError(const char *fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	SDL_LogMessageV(SDL_LOG_CATEGORY_ERROR, SDL_LOG_PRIORITY_ERROR, fmt, args);
	va_end(args);
}

static void LoadFileToBuffer(const char *filename, unsigned char **file_buffer, size_t *file_size)
{
	*file_buffer = NULL;

	FILE *file = fopen(filename, "rb");

	if (file == NULL)
	{
		PrintError("Could not open file");
	}
	else
	{
		fseek(file, 0, SEEK_END);
		*file_size = ftell(file);
		rewind(file);
		*file_buffer = malloc(*file_size);

		if (*file_buffer == NULL)
		{
			PrintError("Could not allocate memory for file");
		}
		else
		{
			fread(*file_buffer, 1, *file_size, file);
		}

		fclose(file);
	}
}


///////////
// Video //
///////////

static SDL_Window *window;
static SDL_Renderer *renderer;
static SDL_Texture *framebuffer_texture;
static Uint32 *framebuffer_texture_pixels;
static int framebuffer_texture_pitch;

static Uint32 colours[3 * 4 * 16];

static unsigned int current_screen_width;
static unsigned int current_screen_height;

static bool use_vsync;

static bool InitVideo(void)
{
	if (SDL_InitSubSystem(SDL_INIT_VIDEO) < 0)
	{
		PrintError("SDL_InitSubSystem(SDL_INIT_VIDEO) failed with the following message - '%s'", SDL_GetError());
	}
	else
	{
		// Create window
		window = SDL_CreateWindow("clownmdemufrontend", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, 320 * 2, 224 * 2, SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI);

		if (window == NULL)
		{
			PrintError("SDL_CreateWindow failed with the following message - '%s'", SDL_GetError());
		}
		else
		{
			// Figure out if we should use V-sync or not
			use_vsync = false;

			SDL_DisplayMode display_mode;
			if (SDL_GetCurrentDisplayMode(SDL_GetWindowDisplayIndex(window), &display_mode) == 0)
				use_vsync = display_mode.refresh_rate >= 60;

			// Create renderer
			renderer = SDL_CreateRenderer(window, -1, use_vsync ? SDL_RENDERER_PRESENTVSYNC : 0);

			if (renderer == NULL)
			{
				PrintError("SDL_CreateRenderer failed with the following message - '%s'", SDL_GetError());
			}
			else
			{
				// Create framebuffer texture
				// We're using ARGB8888 because it's more likely to be supported natively by the GPU, avoiding the need for constant conversions
				framebuffer_texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, 320, 480);

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
					if (SDL_LockTexture(framebuffer_texture, NULL, (void*)&framebuffer_texture_pixels, &framebuffer_texture_pitch) < 0)
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


///////////
// Audio //
///////////

static SDL_AudioDeviceID audio_device;
static size_t audio_buffer_size;

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
	#ifdef PAL
		want.freq = CLOWNMDEMU_MASTER_CLOCK_PAL / 15 / 16;
	#else
		want.freq = CLOWNMDEMU_MASTER_CLOCK_NTSC / 15 / 16;
	#endif
		want.format = AUDIO_S16;
		want.channels = 1;
		// We want a 25ms buffer (this value must be a power of two)
		want.samples = 1;
		while (want.samples < (want.freq * want.channels) / 40) // 25ms
			want.samples <<= 1;
		want.callback = NULL;

		audio_device = SDL_OpenAudioDevice(NULL, 0, &want, &have, 0/*SDL_AUDIO_ALLOW_FREQUENCY_CHANGE*/);

		if (audio_device == 0)
		{
			PrintError("SDL_OpenAudioDevice failed with the following message - '%s'", SDL_GetError());
		}
		else
		{
			audio_buffer_size = have.size;

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

static void ColourUpdatedCallback(unsigned int index, unsigned int colour)
{
	// Decompose XBGR4444 into individual colour channels
	const Uint32 red = (colour >> 4 * 0) & 0xF;
	const Uint32 green = (colour >> 4 * 1) & 0xF;
	const Uint32 blue = (colour >> 4 * 2) & 0xF;

	// Reassemble into ARGB8888
	colours[index] = (blue << 4 * 0) | (blue << 4 * 1) | (green << 4 * 2) | (green << 4 * 3) | (red << 4 * 4) | (red << 4 * 5);
}

static void ScanlineRenderedCallback(unsigned int scanline, const unsigned char *pixels, unsigned int screen_width, unsigned int screen_height)
{
	if (framebuffer_texture_pixels != NULL)
		for (unsigned int i = 0; i < screen_width; ++i)
			framebuffer_texture_pixels[scanline * framebuffer_texture_pitch + i] = colours[pixels[i]];

	current_screen_width = screen_width;
	current_screen_height = screen_height;
}

static cc_bool ReadInputCallback(unsigned int player_id, unsigned int button_id)
{
	assert(player_id < 2);

	cc_bool value = cc_false;

	if (keyboard_input.bound_joypad == player_id)
		value |= keyboard_input.buttons[button_id] ? cc_true : cc_false;

	for (ControllerInput *controller_input = controller_input_list_head; controller_input != NULL; controller_input = controller_input->next)
		if (controller_input->input.bound_joypad == player_id)
			value |= controller_input->input.buttons[button_id] ? cc_true : cc_false;

	return value;
}

static void PSGAudioCallback(short *samples, size_t total_samples)
{
	if (SDL_GetQueuedAudioSize(audio_device) < audio_buffer_size * 2)
		SDL_QueueAudio(audio_device, samples, sizeof(*samples) * total_samples);
}

int main(int argc, char **argv)
{
	if (argc < 2)
	{
		PrintError("Must provide path to input ROM as a parameter");
	}
	else
	{
		// Initialise SDL2
		if (SDL_Init(SDL_INIT_EVENTS | SDL_INIT_GAMECONTROLLER) < 0)
		{
			PrintError("SDL_Init failed with the following message - '%s'", SDL_GetError());
		}
		else
		{
			if (!InitVideo())
			{
				PrintError("InitVideo failed");
			}
			else
			{
				const bool initialised_audio = InitAudio();

				if (!initialised_audio)
					PrintError("InitAudio failed"); // Allow program to continue if audio fails

				ClownMDEmu_Init(&clownmdemu_state);

				// For now, let's emulate a North American console
				ClownMDEmu_SetJapanese(&clownmdemu_state, cc_false);
			#ifdef PAL
				ClownMDEmu_SetPAL(&clownmdemu_state, cc_true);
			#else
				ClownMDEmu_SetPAL(&clownmdemu_state, cc_false);
			#endif

				// Load ROM to memory
				unsigned char *file_buffer;
				size_t file_size;
				LoadFileToBuffer(argv[1], &file_buffer, &file_size);

				if (file_buffer == NULL)
				{
					PrintError("Could not load the ROM");
				}
				else
				{
					ClownMDEmu_UpdateROM(&clownmdemu_state, file_buffer, file_size);
					free(file_buffer);

					ClownMDEmu_Reset(&clownmdemu_state);

					// Initialise save state
					clownmdemu_save_state = clownmdemu_state;

					bool quit = false;

					bool fast_forward = false;

					while (!quit)
					{
						// Process events
						SDL_Event event;
						while (SDL_PollEvent(&event))
						{
							switch (event.type)
							{
								case SDL_QUIT:
									quit = true;
									break;

								case SDL_KEYDOWN:
									// Ignore repeated key inputs caused by holding the key down
									if (event.key.repeat)
										break;

									switch (event.key.keysym.sym)
									{
										case SDLK_TAB:
											// Soft-reset console
											ClownMDEmu_Reset(&clownmdemu_state);
											break;

										case SDLK_F1:
										{
											// Toggle fullscreen
											static bool fullscreen;

											fullscreen = !fullscreen;

											SDL_SetWindowFullscreen(window, fullscreen ? SDL_WINDOW_FULLSCREEN_DESKTOP : 0);
											SDL_ShowCursor(fullscreen ? SDL_DISABLE : SDL_ENABLE);

											break;
										}

										case SDLK_F2:
											// Toggle which joypad the keyboard controls
											keyboard_input.bound_joypad ^= 1;
											break;

										case SDLK_F5:
											// Save save state
											clownmdemu_save_state = clownmdemu_state;
											break;

										case SDLK_F9:
											// Load save state
											clownmdemu_state = clownmdemu_save_state;
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
										#define DO_KEY(state, code) case code: keyboard_input.buttons[state] = pressed; break;

										DO_KEY(CLOWNMDEMU_BUTTON_UP,    SDL_SCANCODE_W)
										DO_KEY(CLOWNMDEMU_BUTTON_DOWN,  SDL_SCANCODE_S)
										DO_KEY(CLOWNMDEMU_BUTTON_LEFT,  SDL_SCANCODE_A)
										DO_KEY(CLOWNMDEMU_BUTTON_RIGHT, SDL_SCANCODE_D)
										DO_KEY(CLOWNMDEMU_BUTTON_A,     SDL_SCANCODE_O)
										DO_KEY(CLOWNMDEMU_BUTTON_B,     SDL_SCANCODE_P)
										DO_KEY(CLOWNMDEMU_BUTTON_C,     SDL_SCANCODE_LEFTBRACKET)
										DO_KEY(CLOWNMDEMU_BUTTON_START, SDL_SCANCODE_RETURN)

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
											ControllerInput *controller_input = calloc(sizeof(ControllerInput), 1);

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
											free(controller_input);
											break;
										}
									}

									break;
								}

								case SDL_CONTROLLERBUTTONDOWN:
								case SDL_CONTROLLERBUTTONUP:
								{
									const bool pressed = event.cbutton.state == SDL_PRESSED;

									for (ControllerInput *controller_input = controller_input_list_head; ; controller_input = controller_input->next)
									{
										if (controller_input == NULL)
										{
											PrintError("Received an SDL_CONTROLLERBUTTONDOWN/SDL_CONTROLLERBUTTONUP event for an unrecognised controller");
											break;
										}

										if (controller_input->joystick_instance_id == event.cbutton.which)
										{
											switch (event.cbutton.button)
											{
												#define DO_BUTTON(state, code) case code: controller_input->input.buttons[state] = pressed; break;

												DO_BUTTON(CLOWNMDEMU_BUTTON_A,     SDL_CONTROLLER_BUTTON_X)
												DO_BUTTON(CLOWNMDEMU_BUTTON_B,     SDL_CONTROLLER_BUTTON_Y)
												DO_BUTTON(CLOWNMDEMU_BUTTON_C,     SDL_CONTROLLER_BUTTON_B)
												DO_BUTTON(CLOWNMDEMU_BUTTON_B,     SDL_CONTROLLER_BUTTON_A)
												DO_BUTTON(CLOWNMDEMU_BUTTON_START, SDL_CONTROLLER_BUTTON_START)

												#undef DO_BUTTON

												case SDL_CONTROLLER_BUTTON_DPAD_UP:
												case SDL_CONTROLLER_BUTTON_DPAD_DOWN:
												case SDL_CONTROLLER_BUTTON_DPAD_LEFT:
												case SDL_CONTROLLER_BUTTON_DPAD_RIGHT:
												{
													unsigned int direction;

													switch (event.cbutton.button)
													{
														case SDL_CONTROLLER_BUTTON_DPAD_UP:
															direction = 0;
															break;

														case SDL_CONTROLLER_BUTTON_DPAD_DOWN:
															direction = 1;
															break;

														case SDL_CONTROLLER_BUTTON_DPAD_LEFT:
															direction = 2;
															break;

														case SDL_CONTROLLER_BUTTON_DPAD_RIGHT:
															direction = 3;
															break;
													}

													const unsigned int buttons[4] = {
														CLOWNMDEMU_BUTTON_UP,
														CLOWNMDEMU_BUTTON_DOWN,
														CLOWNMDEMU_BUTTON_LEFT,
														CLOWNMDEMU_BUTTON_RIGHT
													};

													controller_input->dpad[direction] = pressed;
													controller_input->input.buttons[buttons[direction]] = controller_input->left_stick[direction] || controller_input->dpad[direction];

													break;
												}

												case SDL_CONTROLLER_BUTTON_BACK:
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
									for (ControllerInput *controller_input = controller_input_list_head; ; controller_input = controller_input->next)
									{
										if (controller_input == NULL)
										{
											PrintError("Received an SDL_CONTROLLERAXISMOTION event for an unrecognised controller");
											break;
										}

										if (controller_input->joystick_instance_id == event.caxis.which)
										{
											switch (event.caxis.axis)
											{
												case SDL_CONTROLLER_AXIS_LEFTX:
												case SDL_CONTROLLER_AXIS_LEFTY:
													if (event.caxis.axis == SDL_CONTROLLER_AXIS_LEFTX)
														controller_input->left_stick_x = event.caxis.value;
													else //if (event.caxis.axis == SDL_CONTROLLER_AXIS_LEFTY)
														controller_input->left_stick_y = event.caxis.value;

													const float magnitude = SDL_sqrtf(controller_input->left_stick_x * controller_input->left_stick_x + controller_input->left_stick_y * controller_input->left_stick_y);

													const float left_stick_x_unit = controller_input->left_stick_x / magnitude;
													const float left_stick_y_unit = controller_input->left_stick_y / magnitude;

													for (unsigned int i = 0; i < 4; ++i)
													{
														if (magnitude < 32768.0f / 4.0f)
														{
															controller_input->left_stick[i] = false;
														}
														else
														{
															const float directions[4][2] = {
																{ 0.0f, -1.0f},
																{ 0.0f,  1.0f},
																{-1.0f,  0.0f},
																{ 1.0f,  0.0f}
															};

															const float delta_angle = SDL_acosf(left_stick_x_unit * directions[i][0] + left_stick_y_unit * directions[i][1]);

															controller_input->left_stick[i] = (delta_angle < (360.0f * 3.0f / 8.0f / 2.0f) * (3.14159265358979323846f / 180.0f));
														}

														const unsigned int buttons[4] = {
															CLOWNMDEMU_BUTTON_UP,
															CLOWNMDEMU_BUTTON_DOWN,
															CLOWNMDEMU_BUTTON_LEFT,
															CLOWNMDEMU_BUTTON_RIGHT
														};

														controller_input->input.buttons[buttons[i]] = controller_input->left_stick[i] || controller_input->dpad[i];
													}

													break;

												default:
													break;
											}

											break;
										}
									}

									break;

								default:
									break;
							}
						}

						// Run the emulator for a frame
						ClownMDEmu_Iterate(&clownmdemu_state, &(ClownMDEmu_Callbacks){ColourUpdatedCallback, ScanlineRenderedCallback, ReadInputCallback, PSGAudioCallback});

						// Correct the aspect ratio of the rendered frame
						// (256x224 and 320x240 should be the same width, but 320x224 and 320x240 should be different heights - this matches the behaviour of a real Mega Drive)
						int renderer_width, renderer_height;
						SDL_GetRendererOutputSize(renderer, &renderer_width, &renderer_height);

						SDL_Rect destination_rect;

						if ((unsigned int)renderer_width > renderer_height * 320 / current_screen_height)
						{
							destination_rect.w = renderer_height * 320 / current_screen_height;
							destination_rect.h = renderer_height;
						}
						else
						{
							destination_rect.w = renderer_width;
							destination_rect.h = renderer_width * current_screen_height / 320;
						}

						destination_rect.x = (renderer_width - destination_rect.w) / 2;
						destination_rect.y = (renderer_height - destination_rect.h) / 2;

						// Unlock the texture so that we can draw it
						SDL_UnlockTexture(framebuffer_texture);

						// Draw the rendered frame to the screen
						SDL_RenderClear(renderer);
						SDL_RenderCopy(renderer, framebuffer_texture, &(SDL_Rect){.x = 0, .y = 0, .w = current_screen_width, .h = current_screen_height}, &destination_rect);
						SDL_RenderPresent(renderer);

						// Lock the texture so that we can write to its pixels later
						if (SDL_LockTexture(framebuffer_texture, NULL, (void*)&framebuffer_texture_pixels, &framebuffer_texture_pitch) < 0)
							framebuffer_texture_pixels = NULL;

						framebuffer_texture_pitch /= sizeof(Uint32);

						// Framerate manager
					#ifdef PAL
						#define MULTIPLIER 1
					#else
						// 300 is the magic number that prevents these calculations from ever dipping into numbers smaller than 1
						#define MULTIPLIER 300
					#endif
						static Uint32 next_time;
						const Uint32 current_time = SDL_GetTicks() * MULTIPLIER;

						if (!SDL_TICKS_PASSED(current_time, next_time))
							SDL_Delay((next_time - current_time) / MULTIPLIER);
						else if (SDL_TICKS_PASSED(current_time, next_time + 100 * MULTIPLIER)) // If we're way past our deadline, then resync to the current tick instead of fast-forwarding
							next_time = current_time;

						Uint32 delta;
					#ifdef PAL
						// Run at 50FPS
						delta = CLOWNMDEMU_DIVIDE_BY_PAL_FRAMERATE(1000ul * MULTIPLIER);
					#else
						// Run at roughly 59.94FPS (60 divided by 1.001)
						delta = CLOWNMDEMU_DIVIDE_BY_NTSC_FRAMERATE(1000ul * MULTIPLIER);
					#endif

						next_time += delta >> (fast_forward ? 2 : 0);
					}
				}

				FILE *state_file = fopen("state.bin", "wb");

				if (state_file != NULL)
				{
					fwrite(&clownmdemu_state, 1, sizeof(clownmdemu_state), state_file);

					fclose(state_file);
				}

				ClownMDEmu_Deinit(&clownmdemu_state);

				if (initialised_audio)
					DeinitAudio();

				DeinitVideo();
			}

			SDL_Quit();
		}
	}

	return EXIT_SUCCESS;
}
