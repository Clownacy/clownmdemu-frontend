#ifdef GL_ES
precision mediump int;
precision mediump float;
#endif

varying vec2 fragment_texture_coordinate;

uniform sampler2D palette_texture, screen_texture;

void main()
{
    gl_FragColor = texture2D(palette_texture, vec2(texture2D(screen_texture, fragment_texture_coordinate).x * 4.0 / 3.0, 0.0));
}
