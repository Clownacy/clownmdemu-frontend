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

uniform COMPAT_PRECISION vec2 OutputSize;
uniform COMPAT_PRECISION vec2 OutputSizeAspectCorrected;
uniform COMPAT_PRECISION vec2 TextureSize;
uniform COMPAT_PRECISION vec2 InputSize;
uniform sampler2D texture;

void main()
{
	vec2 texture_coordinate = gl_FragCoord.xy;

	// Un-flip the Y axis.
	texture_coordinate.y = OutputSize.y - texture_coordinate.y;

	// Move from the pixel centre to the top-left coordinate.
	texture_coordinate -= 0.5;

	// Centre the aspect-ratio-corrected view within the window.
	// We floor this because do not want it to cause blurry sub-pixel rendering.
	texture_coordinate -= floor((OutputSize - OutputSizeAspectCorrected) * 0.5);

	// Shrink into the aspect-ratio-corrected view.
	vec2 lower_texture_coordinate = (texture_coordinate + 0.0) * InputSize / OutputSizeAspectCorrected;
	vec2 upper_texture_coordinate = (texture_coordinate + 1.0) * InputSize / OutputSizeAspectCorrected;

	// Ignore texels that are outside the view.
	if (lower_texture_coordinate.x < 0.0 || upper_texture_coordinate.x > InputSize.x || lower_texture_coordinate.y < 0.0 || upper_texture_coordinate.y > InputSize.y)
	{
		FragColor = vec4(0.0, 0.0, 0.0, 1.0);
	}
	else
	{
		// Smooth the edges of the texels by interpolating within the fragment.

		// Compute the interpolation weights.
		// To do this, we measure the space between the upper and lower coordinate bounds,
		// and then determine at which point it crosses an integer boundary.
		// If it does not cross a boundary, then the 'max' function will clamp the output
		// to a sane value.
		// Note that this does not work correctly if the space crosses MULTIPLE integer
		// boundaries, but that would only occur when downscaling.
		vec2 delta = upper_texture_coordinate - lower_texture_coordinate;
		vec2 weight = max(delta - fract(upper_texture_coordinate), 0.0) / delta;

		// Sample the texture, using the weight to control the sub-pixel bilinear filtering.
		FragColor = COMPAT_TEXTURE(texture, (upper_texture_coordinate - fract(upper_texture_coordinate) + 0.5 - weight) / TextureSize);
	}
}
