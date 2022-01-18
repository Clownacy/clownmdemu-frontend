#ifndef CLOWNRESAMPLER_H
#define CLOWNRESAMPLER_H


/* Configuration */

/* Set this to static to limit the visibility of functions. */
#ifndef CLOWNRESAMPLER_API 
#define CLOWNRESAMPLER_API
#endif

/* Controls the number of 'lobes' of the windowed sinc function.
   A higher number results in better audio, but is more expensive. */
#ifndef CLOWNRESAMPLER_KERNEL_RADIUS
#define CLOWNRESAMPLER_KERNEL_RADIUS 3
#endif


/* Header */

#include <stddef.h>


/* Low-level API.
   This API has lower overhead, but is more difficult to use, requiring that
   audio be pre-processed before resampling. */

typedef struct ClownResampler_LowLevel_State
{
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
CLOWNRESAMPLER_API void ClownResampler_LowLevel_Init(ClownResampler_LowLevel_State *resampler);

/* Sets the ratio of the resampler. For example, if the input sample rate is
   48000Hz, then a ratio of 0.5 will cause the output sample rate to be
   24000Hz. It is not recommended to call this function in the middle of
   resampling multiple chunks of one piece of audio, as it resets various parts
   of the resampler's internal state, potentially causing distortion. */
CLOWNRESAMPLER_API void ClownResampler_LowLevel_SetResamplingRatio(ClownResampler_LowLevel_State *resampler, float ratio);

/* Resamples (pre-processed) audio. The 'total_input_samples' and
   'total_output_samples' parameters measure the size of their respective
   buffers in samples, not bytes.

   The input buffer must be specially pre-processed, so that it is padded with
   extra samples at the beginning and end. This is needed as the resampler will
   unavoidably read past the beginning and the end of the audio data. The
   specific number of samples needed at the beginning and end can be found in
   the 'resampler->integer_stretched_kernel_radius' variable. If the audio you
   are resampling is a chunk of a larger piece of audio, then the 'padding' at
   the beginning and end must be the samples from before and after said chunk
   of audio, otherwise these samples should just be 0. Note that these padding
   samples must not be counted by the 'total_input_samples' parameter.

   After this function returns, the 'total_input_samples' parameter will
   contain the number of samples in the input buffer that were not processed.
   Likewise, the 'total_output_samples' parameter will contain the number of
   samples in the output buffer that were not filled with resampled audio data.
*/
CLOWNRESAMPLER_API void ClownResampler_LowLevel_Resample(ClownResampler_LowLevel_State *resampler, const short *input_buffer, size_t *total_input_samples, short *output_buffer, size_t *total_output_samples);


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
   state is passed to any other functions. By default, it will have a
   resampling ratio of 1.0 (that is, the output sample rate will be the same as
   the input). */
CLOWNRESAMPLER_API void ClownResampler_HighLevel_Init(ClownResampler_HighLevel_State *resampler);

/* Sets the ratio of the resampler. For example, if the input sample rate is
   48000Hz, then a ratio of 0.5 will cause the output sample rate to be
   24000Hz. It is not recommended to call this function in the middle of
   resampling multiple chunks of one piece of audio, as it resets various parts
   of the resampler's internal state, potentially causing distortion. */
CLOWNRESAMPLER_API void ClownResampler_HighLevel_SetResamplingRatio(ClownResampler_HighLevel_State *resampler, float ratio);

/* Resamples audio. This function returns when either the output buffer is
   full, or the input callback stops providing samples.

   This function returns the number of samples that were written to the output
   buffer.

   The parameters are as follows:

   'resampler'

   A pointer to a state struct that was previously initialised with the
   'ClownResampler_HighLevel_Init' function.


   'output_buffer'

   A pointer to a buffer which the resampled audio will be written to.
   The size of the audio buffer will be specified by the 'total_output_samples'
   variable.


   'total_output_samples'

   The size of the buffer specified by the 'output_buffer' parameter. The size
   is measured in samples, not bytes.


   'pull_callback'

   A callback for retrieving samples of the input audio. The callback must
   write samples to the buffer pointed to by the 'buffer' parameter. The
   'buffer_size' parameter specifies the maximum number of samples that can be
   written to the buffer. The callback must return the number of samples that
   were written to the buffer. If the callback returns 0, then this function
   terminates. The 'user_data' parameter is the same as the 'user_data'
   parameter of this function.
   
   
   'user_data'
   An arbitrary pointer that is passed to the 'pull_callback' function. */
CLOWNRESAMPLER_API size_t ClownResampler_HighLevel_Resample(ClownResampler_HighLevel_State *resampler, short *output_buffer, size_t total_output_samples, size_t(*pull_callback)(void *user_data, short *buffer, size_t buffer_size), void *user_data);


/* Implementation */

#ifdef CLOWNRESAMPLER_IMPLEMENTATION

#include <assert.h>
#include <math.h>
#include <stddef.h>
#include <string.h>

#define CLOWNRESAMPLER_COUNT_OF(x) (sizeof(x) / sizeof(*(x)))
#define CLOWNRESAMPLER_MIN(a, b) ((a) < (b) ? (a) : (b))
#define CLOWNRESAMPLER_MAX(a, b) ((a) > (b) ? (a) : (b))

static float ClownResampler_LanczosKernel(float x)
{
	const float kernel_radius = (float)CLOWNRESAMPLER_KERNEL_RADIUS;

	assert(x != 0.0f);
/*	if (x == 0.0f)
		return 1.0f;*/

	assert(fabsf(x) <= kernel_radius * 1.001f); /* A slight margin of rounding error */
/*	if (fabsf(x) > kernel_radius)
		return 0.0f;*/

	const float x_times_pi = x * 3.1415926535897932384626433832795028841971693993751058209749445923078164062862089986280348253421170679f; /* 100 digits should be good enough */
	const float x_times_pi_divided_by_radius = x_times_pi / kernel_radius;

	return (sinf(x_times_pi) * sinf(x_times_pi_divided_by_radius)) / (x_times_pi * x_times_pi_divided_by_radius);
}


/* Low-level API */

CLOWNRESAMPLER_API void ClownResampler_LowLevel_Init(ClownResampler_LowLevel_State *resampler)
{
	resampler->position = 0.0f;
	ClownResampler_LowLevel_SetResamplingRatio(resampler, 1.0f); /* A nice sane default */
}

CLOWNRESAMPLER_API void ClownResampler_LowLevel_SetResamplingRatio(ClownResampler_LowLevel_State *resampler, float ratio)
{
	/* TODO - Freak-out if the ratio is so high that the kernel radius would exceed the size of the input buffer */

	const float kernel_scale = CLOWNRESAMPLER_MAX(ratio, 1.0f);

	assert(ratio != 0.0f); /* I would check if the ratio is NAN or INF, but I can't do that in C89 */

	resampler->increment = ratio;
	resampler->stretched_kernel_radius = (float)CLOWNRESAMPLER_KERNEL_RADIUS * kernel_scale;
	resampler->integer_stretched_kernel_radius = (size_t)ceilf(resampler->stretched_kernel_radius);
	resampler->inverse_kernel_scale = 1.0f / kernel_scale;
}

CLOWNRESAMPLER_API void ClownResampler_LowLevel_Resample(ClownResampler_LowLevel_State *resampler, const short *input_buffer, size_t *total_input_samples, short *output_buffer, size_t *total_output_samples)
{
	short *output_buffer_pointer = output_buffer;
	short *output_buffer_end = output_buffer + *total_output_samples;

	for (;;)
	{
		const size_t audio_resampler_position_integer = (size_t)resampler->position;
		const float audio_resampler_position_fractional = resampler->position - (float)audio_resampler_position_integer;

		/* Check if we've reached the end of the input buffer. */
		if (audio_resampler_position_integer >= *total_input_samples)
		{
			resampler->position -= *total_input_samples;
			*total_input_samples = 0;
			break;
		}
		/* Check if we've reached the end of the output buffer. */
		else if (output_buffer_pointer >= output_buffer_end)
		{
			resampler->position -= audio_resampler_position_integer;
			*total_input_samples -= audio_resampler_position_integer;
			break;
		}

		/* This avoids the need for ClownResampler_LanczosKernel to return 1.0f if its parameter is 0.0f. */
		if (audio_resampler_position_fractional == 0.0f)
		{
			assert(audio_resampler_position_integer < *total_input_samples);

			*output_buffer_pointer = input_buffer[resampler->integer_stretched_kernel_radius + audio_resampler_position_integer];
		}
		else
		{
			int i;

			float sample = 0.0f;
			float level = 0.0f;

			/* TODO - size_t */
			const int min = (int)(resampler->position - resampler->stretched_kernel_radius + 1.0f); /* Essentially rounding up (with an acceptable slight margin of error) */
			const int max = (int)(resampler->position + resampler->stretched_kernel_radius);        /* Will round down on its own */

			assert(min > -(int)resampler->integer_stretched_kernel_radius);
			assert(max < *total_input_samples + resampler->integer_stretched_kernel_radius);

			for (i = min; i <= max; ++i)
			{
				const float kernel_value = ClownResampler_LanczosKernel(((float)i - resampler->position) * resampler->inverse_kernel_scale);
				level += kernel_value;

				sample += (float)input_buffer[resampler->integer_stretched_kernel_radius + i] * kernel_value;
			}

			*output_buffer_pointer = (short)(sample / level); /* Normalise */
		}

		++output_buffer_pointer;

		resampler->position += resampler->increment;
	}

	*total_output_samples -= output_buffer_pointer - output_buffer;
}


/* High-level API */

CLOWNRESAMPLER_API void ClownResampler_HighLevel_Init(ClownResampler_HighLevel_State *resampler)
{
	ClownResampler_LowLevel_Init(&resampler->low_level);
	ClownResampler_HighLevel_SetResamplingRatio(resampler, 1.0f);

	resampler->input_buffer_start = resampler->input_buffer;
	resampler->input_buffer_end = resampler->input_buffer;
}

CLOWNRESAMPLER_API void ClownResampler_HighLevel_SetResamplingRatio(ClownResampler_HighLevel_State *resampler, float ratio)
{
	ClownResampler_LowLevel_SetResamplingRatio(&resampler->low_level, ratio);

	memset(resampler->input_buffer, 0, resampler->low_level.integer_stretched_kernel_radius * 2 * sizeof(*resampler->input_buffer));
}

CLOWNRESAMPLER_API size_t ClownResampler_HighLevel_Resample(ClownResampler_HighLevel_State *resampler, short *output_buffer, size_t total_output_samples, size_t(*pull_callback)(void *user_data, short *buffer, size_t buffer_size), void *user_data)
{
	short *output_buffer_start = output_buffer;
	short *output_buffer_end = output_buffer_start + total_output_samples;

	/* If we've run out of room in the output buffer, quit. */
	while (output_buffer_start != output_buffer_end)
	{
		size_t input_samples;
		size_t output_samples;

		/* If the input buffer is empty, refill it. */
		if (resampler->input_buffer_start == resampler->input_buffer_end)
		{
			/* It's hard to explain this step-by-step, but essentially there's a trick we do here:
			   in order to avoid the resampler reading samples outside of the buffer, we have 'deadzones'
			   at each end of the buffer. When a new batch of samples is needed, the second deadzone is
			   copied over the first one, and the second is overwritten by the end of the new samples. */
			const size_t double_radius = resampler->low_level.integer_stretched_kernel_radius * 2;

			/* Move the end of the last batch of data to the start of the buffer */
			/* (memcpy won't work here since the copy may overlap). */
			memmove(resampler->input_buffer, resampler->input_buffer_end - resampler->low_level.integer_stretched_kernel_radius, double_radius * sizeof(*resampler->input_buffer));

			/* Obtain input samples (note that the new samples start after the samples we just copied). */
			resampler->input_buffer_start = resampler->input_buffer + resampler->low_level.integer_stretched_kernel_radius;
			resampler->input_buffer_end = resampler->input_buffer_start + pull_callback(user_data, resampler->input_buffer + double_radius, CLOWNRESAMPLER_COUNT_OF(resampler->input_buffer) - double_radius);
			input_samples = pull_callback(user_data, resampler->input_buffer + double_radius, CLOWNRESAMPLER_COUNT_OF(resampler->input_buffer) - double_radius);

			/* If the callback returns 0, then we must have reached the end of the input data, so quit. */
			if (resampler->input_buffer_start == resampler->input_buffer_end)
				break;
		}

		/* Call the actual resampler. */
		input_samples = resampler->input_buffer_end - resampler->input_buffer_start;
		output_samples = output_buffer_end - output_buffer_start;
		ClownResampler_LowLevel_Resample(&resampler->low_level, resampler->input_buffer_start - resampler->low_level.integer_stretched_kernel_radius, &input_samples, output_buffer_start, &output_samples);

		/* Increment input and output pointers. */
		resampler->input_buffer_start = resampler->input_buffer_end - input_samples;
		output_buffer_start = output_buffer_end - output_samples;
	}

	return total_output_samples - (output_buffer_end - output_buffer_start);
}

#endif /* CLOWNRESAMPLER_IMPLEMENTATION */

#endif /* CLOWNRESAMPLER_H */
