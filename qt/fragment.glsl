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

varying vec2 TextureCoordinate;
uniform vec2 InputSize;
uniform vec2 TextureSize;
uniform sampler2D Texture0;

void main()
{
	vec2 texel_coord = floor(TextureCoordinate * InputSize) + 0.5;
	FragColor = COMPAT_TEXTURE(Texture0, texel_coord / TextureSize);
}
