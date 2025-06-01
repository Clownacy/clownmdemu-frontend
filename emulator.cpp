#include "emulator.h"

#include <array>

#include <QMessageBox>

struct Vertex
{
    std::array<GLbyte, 2> position;
    std::array<GLubyte, 2> texture_coordinate;
};

static constexpr auto vertices = std::to_array<Vertex>({
    {{-0x80, -0x80}, {0x00, 0xFF}},
    {{ 0x7F, -0x80}, {0xFF, 0xFF}},
    {{-0x80,  0x7F}, {0x00, 0x00}},
    {{ 0x7F,  0x7F}, {0xFF, 0x00}}
});

static constexpr const char *attribute_name_vertex_position = "vertex_position";
static constexpr const char *attribute_name_vertex_texture_coordinate = "vertex_texture_coordinate";

void Emulator::initializeGL()
{
    initializeOpenGLFunctions();

#define DisplayError(MESSAGE) QMessageBox::critical(this, "Fatal Error", "Failed to " MESSAGE "; emulator will not be displayed!")

    shader_program.emplace();

    if (!shader_program->addShaderFromSourceFile(QOpenGLShader::Vertex, ":/vertex.glsl"))
        DisplayError("load vertex shader");

    if (!shader_program->addShaderFromSourceFile(QOpenGLShader::Fragment, ":/fragment.glsl"))
        DisplayError("load fragment shader");

    if (!shader_program->link())
        DisplayError("link shader program");

    if (!shader_program->bind())
        DisplayError("bind shader program");

    if (!vertex_buffer_object.create())
        DisplayError("create vertex buffer object");

    vertex_buffer_object.bind();
    vertex_buffer_object.setUsagePattern(QOpenGLBuffer::StaticDraw);
    vertex_buffer_object.allocate(std::data(vertices), sizeof(vertices));

#define SetAttributeBuffer(PROGRAM, LOCATION, TYPE, STRUCT, MEMBER) (PROGRAM)->setAttributeBuffer(LOCATION, TYPE, offsetof(STRUCT, MEMBER), sizeof(STRUCT::MEMBER) / sizeof(STRUCT::MEMBER[0]), sizeof(STRUCT))

    SetAttributeBuffer(shader_program, attribute_name_vertex_position, GL_BYTE, Vertex, position);
    SetAttributeBuffer(shader_program, attribute_name_vertex_texture_coordinate, GL_UNSIGNED_BYTE, Vertex, texture_coordinate);

#undef SetAttributeBuffer

    if (!texture.create())
        DisplayError("create texture");

    texture.setFormat(QOpenGLTexture::RGBAFormat);
    texture.setMinificationFilter(QOpenGLTexture::Nearest);
    texture.setMagnificationFilter(QOpenGLTexture::Nearest);
    texture.setSize(2, 2);
    texture.allocateStorage(QOpenGLTexture::RGBA, QOpenGLTexture::UInt8);
    static const unsigned char pixels[] = {
        0xFF, 0x00, 0x00, 0xFF,
        0x00, 0xFF, 0x00, 0xFF,
        0x00, 0x00, 0xFF, 0xFF,
        0xFF, 0xFF, 0xFF, 0xFF,
    };
    texture.setData(QOpenGLTexture::RGBA, QOpenGLTexture::UInt8, pixels);

#undef DisplayError
}

void Emulator::paintGL()
{
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);

    shader_program->bind();
    vertex_buffer_object.bind();
    texture.bind();

    shader_program->enableAttributeArray(attribute_name_vertex_position);
    shader_program->enableAttributeArray(attribute_name_vertex_texture_coordinate);

    glClear(GL_COLOR_BUFFER_BIT);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, std::size(vertices));
}

Emulator::~Emulator()
{
    makeCurrent();

    texture.destroy();
    vertex_buffer_object.destroy();
    shader_program.reset();

    doneCurrent();
}
