#ifdef GL_ES
precision mediump int;
precision mediump float;
#endif

varying vec2 fragment_texture_coordinate;

uniform sampler2D texture;

void main()
{
    gl_FragColor = texture2D(texture, fragment_texture_coordinate);
}
