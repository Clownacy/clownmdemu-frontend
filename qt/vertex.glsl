#ifdef GL_ES
precision mediump int;
precision mediump float;
#endif

attribute vec4 vertex_position;

uniform vec2 texture_coordinate_scale;

void main()
{
	gl_Position = vertex_position;
}
