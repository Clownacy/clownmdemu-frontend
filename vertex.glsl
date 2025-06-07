#ifdef GL_ES
precision mediump int;
precision mediump float;
#endif

attribute vec4 vertex_position;
attribute vec2 vertex_texture_coordinate;

varying vec2 fragment_texture_coordinate;

uniform vec2 texture_coordinate_scale;

void main()
{
    gl_Position = vertex_position;
    fragment_texture_coordinate = vertex_texture_coordinate * texture_coordinate_scale;
}
