#ifdef GL_ES
#ifdef GL_FRAGMENT_PRECISION_HIGH
precision highp float;
#else
precision mediump float;
#endif
#define COMPAT_PRECISION mediump
#else
#define COMPAT_PRECISION
#endif

#if __VERSION__ >= 130
#define COMPAT_VARYING in
#define COMPAT_TEXTURE texture
out COMPAT_PRECISION vec4 FragColor;
#else
#define COMPAT_VARYING varying
#define FragColor gl_FragColor
#define COMPAT_TEXTURE texture2D
#endif

uniform COMPAT_PRECISION vec2 OutputSize, InverseOutputSize;
uniform COMPAT_PRECISION vec2 OutputSizeAspectCorrected, InverseOutputSizeAspectCorrected;
uniform COMPAT_PRECISION vec2 TextureSize, InverseTextureSize;
uniform COMPAT_PRECISION vec2 InputSize, InverseInputSize;
uniform sampler2D texture;

void main()
{
	vec2 texture_coordinate_pixels = gl_FragCoord.xy;

	// Un-flip the Y axis.
	texture_coordinate_pixels.y = OutputSize.y - texture_coordinate_pixels.y;

	// Move from the pixel centre to the top-left coordinate.
	texture_coordinate_pixels -= 0.5;

	// Centre the aspect-ratio-corrected view within the window.
	// We floor this because do not want it to cause blurry sub-pixel rendering.
	texture_coordinate_pixels -= floor((OutputSize - OutputSizeAspectCorrected) * 0.5);

	// Shrink into the aspect-ratio-corrected view.
	vec2 step_size = InputSize * InverseOutputSizeAspectCorrected;
	texture_coordinate_pixels *= step_size;

	// Ignore texels that are outside the view.
	if (texture_coordinate_pixels.x < 0.0 || texture_coordinate_pixels.x >= InputSize.x || texture_coordinate_pixels.y < 0.0 || texture_coordinate_pixels.y >= InputSize.y)
	{
		FragColor = vec4(0.0, 0.0, 0.0, 1.0);
	}
	else
	{
		// Smooth the edges of the texels by interpolating within the fragment.
		vec2 lower_texture_coordinate_pixels = texture_coordinate_pixels;
		vec2 upper_texture_coordinate_pixels = texture_coordinate_pixels + step_size;

		vec2 lower_texture_coordinate = lower_texture_coordinate_pixels * InverseTextureSize;
		vec2 upper_texture_coordinate = upper_texture_coordinate_pixels * InverseTextureSize;

		// Compute the interpolation weights.
		// To do this, we measure the space between the upper and lower coordinate bounds,
		// and then determine at which point it crosses an integer boundary.
		// If it does not cross a boundary, then the 'max' function will clamp the output
		// to a sane value.
		// Note that this does not work correctly if the space crosses MULTIPLE integer
		// boundaries, but that would only occur when downscaling.
		vec2 delta = upper_texture_coordinate_pixels - lower_texture_coordinate_pixels;
		vec2 weight = max(delta - fract(upper_texture_coordinate_pixels), 0.0) / delta;

		// Sample all four corners of the screen pixel...
		vec4 top_left = COMPAT_TEXTURE(texture, vec2(lower_texture_coordinate.x, lower_texture_coordinate.y));
		vec4 top_right = COMPAT_TEXTURE(texture, vec2(upper_texture_coordinate.x, lower_texture_coordinate.y));
		vec4 bottom_left = COMPAT_TEXTURE(texture, vec2(lower_texture_coordinate.x, upper_texture_coordinate.y));
		vec4 bottom_right = COMPAT_TEXTURE(texture, vec2(upper_texture_coordinate.x, upper_texture_coordinate.y));

		// ...then interpolate and output.
		// Note that the weight is backwards (1.0 instead of 0.0 and vice versa), so we
		// reverse the arguments to the mix function to compensate.
		vec4 top = mix(top_right, top_left, weight.x);
		vec4 bottom = mix(bottom_right, bottom_left, weight.x);
		FragColor = mix(bottom, top, weight.y);
	}
}
