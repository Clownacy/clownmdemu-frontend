#ifdef GL_ES
precision mediump int;
precision mediump float;
#endif

attribute vec4 vertex_position;

varying vec2 TextureCoordinate;
uniform vec2 OutputSize;
uniform vec2 OutputSizeAspectCorrected;

void main()
{
	gl_Position = vec4(vertex_position.xy * (OutputSizeAspectCorrected / OutputSize), 0.0, 1.0);
	TextureCoordinate = vec2(vertex_position.x * 0.5 + 0.5, 0.5 - vertex_position.y * 0.5);
}
