/* Public-domain C89 single-file library for resampling audio.
   Made by Clownacy. */

#ifndef CLOWNRESAMPLER_H
#define CLOWNRESAMPLER_H


/* Configuration */

/* Define this as 'static' to limit the visibility of functions. */
#ifndef CLOWNRESAMPLER_API 
#define CLOWNRESAMPLER_API
#endif

/* Controls the number of 'lobes' of the windowed sinc function.
   A higher number results in better audio, but is more expensive. */
#ifndef CLOWNRESAMPLER_KERNEL_RADIUS
#define CLOWNRESAMPLER_KERNEL_RADIUS 3
#endif

#ifndef CLOWNRESAMPLER_MAXIMUM_CHANNELS
#define CLOWNRESAMPLER_MAXIMUM_CHANNELS 2
#endif

/* Header */

#include <stddef.h>


/* Low-level API.
   This API has lower overhead, but is more difficult to use, requiring that
   audio be pre-processed before resampling. */

typedef struct ClownResampler_LowLevel_State
{
	unsigned int channels;
	float position;
	float increment;
	float stretched_kernel_radius;
	size_t integer_stretched_kernel_radius;
	float inverse_kernel_scale;
} ClownResampler_LowLevel_State;

/* Initialises a low-level resampler. This function must be called before the
   state is passed to any other functions. By default, it will have a
   resampling ratio of 1.0 (that is, the output sample rate will be the same as
   the input). */
CLOWNRESAMPLER_API void ClownResampler_LowLevel_Init(ClownResampler_LowLevel_State *resampler, unsigned int channels);

/* Sets the ratio of the resampler. For example, if the input sample rate is
   48000Hz, then a ratio of 0.5 will cause the output sample rate to be
   24000Hz. */
CLOWNRESAMPLER_API void ClownResampler_LowLevel_SetResamplingRatio(ClownResampler_LowLevel_State *resampler, float ratio);

/* Resamples (pre-processed) audio. The 'total_input_frames' and
   'total_output_frames' parameters measure the size of their respective
   buffers in frames, not samples or bytes.

   The input buffer must be specially pre-processed, so that it is padded with
   extra frames at the beginning and end. This is needed as the resampler will
   unavoidably read past the beginning and the end of the audio data. The
   specific number of frames needed at the beginning and end can be found in
   the 'resampler->integer_stretched_kernel_radius' variable. If the audio you
   are resampling is a chunk of a larger piece of audio, then the 'padding' at
   the beginning and end must be the frames from before and after said chunk
   of audio, otherwise these frames should just be 0. Note that these padding
   frames must not be counted by the 'total_input_frames' parameter.

   After this function returns, the 'total_input_frames' parameter will
   contain the number of frames in the input buffer that were not processed.
   Likewise, the 'total_output_frames' parameter will contain the number of
   frames in the output buffer that were not filled with resampled audio data.
*/
CLOWNRESAMPLER_API void ClownResampler_LowLevel_Resample(ClownResampler_LowLevel_State *resampler, const short *input_buffer, size_t *total_input_frames, short *output_buffer, size_t *total_output_frames);


/* High-level API.
   This API has more overhead, but is easier to use. */

typedef struct ClownResampler_HighLevel_State
{
	ClownResampler_LowLevel_State low_level;

	short input_buffer[0x1000];
	short *input_buffer_start;
	short *input_buffer_end;
} ClownResampler_HighLevel_State;

/* Initialises a high-level resampler. This function must be called before the
   state is passed to any other functions. This function also sets the ratio of
   the resampler. For example, if the input sample rate is 48000Hz, then a
   ratio of 0.5 will cause the output sample rate to be 24000Hz. */
CLOWNRESAMPLER_API void ClownResampler_HighLevel_Init(ClownResampler_HighLevel_State *resampler, unsigned int channels, float ratio);

/* Resamples audio. This function returns when either the output buffer is
   full, or the input callback stops providing frames.

   This function returns the number of frames that were written to the output
   buffer.

   The parameters are as follows:

   'resampler'

   A pointer to a state struct that was previously initialised with the
   'ClownResampler_HighLevel_Init' function.


   'output_buffer'

   A pointer to a buffer which the resampled audio will be written to.
   The size of the audio buffer will be specified by the 'total_output_frames'
   variable.


   'total_output_frames'

   The size of the buffer specified by the 'output_buffer' parameter. The size
   is measured in frames, not samples or bytes.


   'pull_callback'

   A callback for retrieving frames of the input audio. The callback must
   write frames to the buffer pointed to by the 'buffer' parameter. The
   'buffer_size' parameter specifies the maximum number of frames that can be
   written to the buffer. The callback must return the number of frames that
   were written to the buffer. If the callback returns 0, then this function
   terminates. The 'user_data' parameter is the same as the 'user_data'
   parameter of this function.
   
   
   'user_data'
   An arbitrary pointer that is passed to the 'pull_callback' function. */
CLOWNRESAMPLER_API size_t ClownResampler_HighLevel_Resample(ClownResampler_HighLevel_State *resampler, short *output_buffer, size_t total_output_frames, size_t(*pull_callback)(void *user_data, short *buffer, size_t buffer_size), void *user_data);


/* Implementation */

#ifdef CLOWNRESAMPLER_IMPLEMENTATION

#include <assert.h>
#include <math.h>
#include <stddef.h>
#include <string.h>

#define CLOWNRESAMPLER_COUNT_OF(x) (sizeof(x) / sizeof(*(x)))
#define CLOWNRESAMPLER_MIN(a, b) ((a) < (b) ? (a) : (b))
#define CLOWNRESAMPLER_MAX(a, b) ((a) > (b) ? (a) : (b))
#define CLOWNRESAMPLER_CLAMP(x, min, max) (CLOWNRESAMPLER_MIN((max), CLOWNRESAMPLER_MAX((min), (x))))

static float ClownResampler_LanczosKernel(float x)
{
	const float kernel_radius = (float)CLOWNRESAMPLER_KERNEL_RADIUS;

	assert(x != 0.0f);
	/*if (x == 0.0f)
		return 1.0f;*/

	assert(fabsf(x) <= kernel_radius * 1.001f); /* A slight margin of rounding error */
	/*if (fabsf(x) > kernel_radius)
		return 0.0f;*/

	const float x_times_pi = x * 3.1415926535897932384626433832795028841971693993751058209749445923078164062862089986280348253421170679f; /* 100 digits should be good enough */
	const float x_times_pi_divided_by_radius = x_times_pi / kernel_radius;

	return (sinf(x_times_pi) * sinf(x_times_pi_divided_by_radius)) / (x_times_pi * x_times_pi_divided_by_radius);
}


/* Low-level API */

CLOWNRESAMPLER_API void ClownResampler_LowLevel_Init(ClownResampler_LowLevel_State *resampler, unsigned int channels)
{
	/* TODO - We really should just return here */
	assert(channels <= CLOWNRESAMPLER_MAXIMUM_CHANNELS);

	resampler->channels = channels;
	resampler->position = 0.0f;
	resampler->integer_stretched_kernel_radius = 0; /* This is needed for the position bias logic in the following function */
	ClownResampler_LowLevel_SetResamplingRatio(resampler, 1.0f); /* A nice sane default */
}

CLOWNRESAMPLER_API void ClownResampler_LowLevel_SetResamplingRatio(ClownResampler_LowLevel_State *resampler, float ratio)
{
	/* TODO - Freak-out if the ratio is so high that the kernel radius would exceed the size of the input buffer */
	const float kernel_scale = CLOWNRESAMPLER_MAX(ratio, 1.0f);

	assert(ratio != 0.0f); /* I would check if the ratio is NAN or INF, but I can't do that in C89 */

	/* Undo the position bias. */
	resampler->position -= (float)resampler->integer_stretched_kernel_radius;

	resampler->increment = ratio;
	resampler->stretched_kernel_radius = (float)CLOWNRESAMPLER_KERNEL_RADIUS * kernel_scale;
	resampler->integer_stretched_kernel_radius = (size_t)ceilf(resampler->stretched_kernel_radius);
	resampler->inverse_kernel_scale = 1.0f / kernel_scale;

	/* Reapply the position bias. */
	resampler->position += (float)resampler->integer_stretched_kernel_radius;
}

CLOWNRESAMPLER_API void ClownResampler_LowLevel_Resample(ClownResampler_LowLevel_State *resampler, const short *input_buffer, size_t *total_input_frames, short *output_buffer, size_t *total_output_frames)
{
	/* Account for the position bias. */
	const size_t max_position = *total_input_frames + resampler->integer_stretched_kernel_radius;

	short *output_buffer_pointer = output_buffer;
	short *output_buffer_end = output_buffer + *total_output_frames * resampler->channels;

	for (;;)
	{
		unsigned int current_channel;

		/* To my knowledge, this is the fastest way to obtain the integral and fractional components of a float. */
		const size_t position_integer = (size_t)resampler->position;
		const float position_fractional = resampler->position - (float)position_integer;

		/* Check if we've reached the end of the input buffer. */
		if (position_integer >= max_position)
		{
			resampler->position -= *total_input_frames;
			*total_input_frames = 0;
			break;
		}
		/* Check if we've reached the end of the output buffer. */
		else if (output_buffer_pointer >= output_buffer_end)
		{
			const size_t position_minus_bias = position_integer - resampler->integer_stretched_kernel_radius;

			resampler->position -= position_minus_bias;
			*total_input_frames -= position_minus_bias;
			break;
		}

		/* This avoids the need for the kernel to return 1.0f if its parameter is 0.0f. */
		if (position_fractional == 0.0f)
		{
			assert(position_integer < max_position);

			for (current_channel = 0; current_channel < resampler->channels; ++current_channel)
				*output_buffer_pointer++ = input_buffer[position_integer * resampler->channels + current_channel];
		}
		else
		{
			size_t i;

			float samples[CLOWNRESAMPLER_MAXIMUM_CHANNELS] = {0.0f}; /* Sample accumulators */

			/* Calculate the bounds of the kernel convolution. */
			const size_t min = (size_t)(resampler->position - resampler->stretched_kernel_radius + 1.0f); /* Essentially rounding up (with an acceptable slight margin of error) */
			const size_t max = (size_t)(resampler->position + resampler->stretched_kernel_radius);        /* Will round down on its own */

			assert(min < *total_input_frames + resampler->integer_stretched_kernel_radius * 2);
			assert(max < *total_input_frames + resampler->integer_stretched_kernel_radius * 2);

			for (i = min; i <= max; ++i)
			{
				/* The distance between the frames being output and the frames being read are the parameter to the Lanczos kernel. */
				const float kernel_value = ClownResampler_LanczosKernel(((float)i - resampler->position) * resampler->inverse_kernel_scale);

				/* Modulate the samples with the kernel and add it to the accumulator. */
				for (current_channel = 0; current_channel < resampler->channels; ++current_channel)
					samples[current_channel] += (float)input_buffer[i * resampler->channels + current_channel] * kernel_value;
			}

			/* Perform clamping and output samples. */
			for (current_channel = 0; current_channel < resampler->channels; ++current_channel)
				*output_buffer_pointer++ = (short)CLOWNRESAMPLER_CLAMP(samples[current_channel], -32768.0f, 32767.0f);
		}

		/* Increment input buffer position. */
		resampler->position += resampler->increment;
	}

	/* Make 'total_output_frames' reflect how much space there is left in the output buffer. */
	*total_output_frames -= (output_buffer_pointer - output_buffer) / resampler->channels;
}


/* High-level API */

CLOWNRESAMPLER_API void ClownResampler_HighLevel_Init(ClownResampler_HighLevel_State *resampler, unsigned int channels, float ratio)
{
	ClownResampler_LowLevel_Init(&resampler->low_level, channels);
	ClownResampler_LowLevel_SetResamplingRatio(&resampler->low_level, ratio);

	memset(resampler->input_buffer, 0, resampler->low_level.integer_stretched_kernel_radius * 2 * sizeof(*resampler->input_buffer));
	resampler->input_buffer_start = resampler->input_buffer_end = resampler->input_buffer + resampler->low_level.integer_stretched_kernel_radius;
}

CLOWNRESAMPLER_API size_t ClownResampler_HighLevel_Resample(ClownResampler_HighLevel_State *resampler, short *output_buffer, size_t total_output_frames, size_t(*pull_callback)(void *user_data, short *buffer, size_t buffer_size), void *user_data)
{
	short *output_buffer_start = output_buffer;
	short *output_buffer_end = output_buffer_start + total_output_frames * resampler->low_level.channels;

	/* If we've run out of room in the output buffer, quit. */
	while (output_buffer_start != output_buffer_end)
	{
		const size_t radius_in_samples = resampler->low_level.integer_stretched_kernel_radius * resampler->low_level.channels;

		size_t input_frames;
		size_t output_frames;

		/* If the input buffer is empty, refill it. */
		if (resampler->input_buffer_start == resampler->input_buffer_end)
		{
			/* It's hard to explain this step-by-step, but essentially there's a trick we do here:
			   in order to avoid the resampler reading frames outside of the buffer, we have 'deadzones'
			   at each end of the buffer. When a new batch of frames is needed, the second deadzone is
			   copied over the first one, and the second is overwritten by the end of the new frames. */
			const size_t double_radius_in_samples = radius_in_samples * 2;

			/* Move the end of the last batch of data to the start of the buffer */
			/* (memcpy won't work here since the copy may overlap). */
			memmove(resampler->input_buffer, resampler->input_buffer_end - radius_in_samples, double_radius_in_samples * sizeof(*resampler->input_buffer));

			/* Obtain input frames (note that the new frames start after the frames we just copied). */
			resampler->input_buffer_start = resampler->input_buffer + radius_in_samples;
			resampler->input_buffer_end = resampler->input_buffer_start + pull_callback(user_data, resampler->input_buffer + double_radius_in_samples, (CLOWNRESAMPLER_COUNT_OF(resampler->input_buffer) - double_radius_in_samples) / resampler->low_level.channels) * resampler->low_level.channels;

			/* If the callback returns 0, then we must have reached the end of the input data, so quit. */
			if (resampler->input_buffer_start == resampler->input_buffer_end)
				break;
		}

		/* Call the actual resampler. */
		input_frames = (resampler->input_buffer_end - resampler->input_buffer_start) / resampler->low_level.channels;
		output_frames = (output_buffer_end - output_buffer_start) / resampler->low_level.channels;
		ClownResampler_LowLevel_Resample(&resampler->low_level, resampler->input_buffer_start - radius_in_samples, &input_frames, output_buffer_start, &output_frames);

		/* Increment input and output pointers. */
		resampler->input_buffer_start = resampler->input_buffer_end - input_frames * resampler->low_level.channels;
		output_buffer_start = output_buffer_end - output_frames * resampler->low_level.channels;
	}

	return total_output_frames - (output_buffer_end - output_buffer_start) / resampler->low_level.channels;
}

#endif /* CLOWNRESAMPLER_IMPLEMENTATION */

#endif /* CLOWNRESAMPLER_H */
