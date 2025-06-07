#include "emulator-widget.h"

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

void EmulatorWidget::initializeGL()
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
        DisplayError("create screen texture");

    texture.setFormat(QOpenGLTexture::RGBFormat);
    texture.setMinificationFilter(QOpenGLTexture::Nearest);
    texture.setMagnificationFilter(QOpenGLTexture::Nearest);
    texture.setSize(texture_buffer_width, texture_buffer_height);
    texture.allocateStorage(QOpenGLTexture::RGB, QOpenGLTexture::UInt16_R5G6B5);

#undef DisplayError
}

void EmulatorWidget::paintGL()
{
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);

    shader_program->bind();
    vertex_buffer_object.bind();
    texture.bind();

    shader_program->enableAttributeArray(attribute_name_vertex_position);
    shader_program->enableAttributeArray(attribute_name_vertex_texture_coordinate);
    shader_program->setUniformValue("texture_coordinate_scale", QVector2D(screen_width, screen_height) / QVector2D(texture_buffer_width, texture_buffer_height));

    glClear(GL_COLOR_BUFFER_BIT);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, std::size(vertices));
}

EmulatorWidget::EmulatorWidget(QWidget* const parent, const Qt::WindowFlags f)
    : QOpenGLWidget(parent, f)
    , Emulator(configuration, constant, state)
{}

EmulatorWidget::~EmulatorWidget()
{
    makeCurrent();

    texture.destroy();
    vertex_buffer_object.destroy();
    shader_program.reset();

    doneCurrent();
}

cc_u8f EmulatorWidget::CartridgeRead(const cc_u32f address)
{
    if (static_cast<qsizetype>(address) >= cartridge_rom_buffer.size())
        return 0;

    return AccessCartridgeBuffer(address);
}

void EmulatorWidget::CartridgeWritten(const cc_u32f address, const cc_u8f value)
{
    if (static_cast<qsizetype>(address) >= cartridge_rom_buffer.size())
        return;

    AccessCartridgeBuffer(address) = value;
}

void EmulatorWidget::ColourUpdated(const cc_u16f index, const cc_u16f colour)
{
    const auto Extract4BitChannelTo6Bit = [&](const unsigned int channel_index)
    {
        const cc_u8f channel = (colour >> (channel_index * 4)) & 0xF;
        return channel << 2 | channel >> 2;
    };

    // Unpack from XBGR4444.
    const auto red   = Extract4BitChannelTo6Bit(0) >> 1;
    const auto green = Extract4BitChannelTo6Bit(1);
    const auto blue  = Extract4BitChannelTo6Bit(2) >> 1;

    // Pack into RGB565.
    palette[index]
            = (red   << (0 + 6 + 5))
            | (green << (0 + 0 + 5))
            | (blue  << (0 + 0 + 0));
}

void EmulatorWidget::ScanlineRendered(const cc_u16f scanline, const cc_u8l* const pixels, const cc_u16f left_boundary, const cc_u16f right_boundary, const cc_u16f screen_width, const cc_u16f screen_height)
{
    for (cc_u16f i = 0; i < right_boundary - left_boundary; ++i)
        texture_buffer[scanline][left_boundary + i] = palette[pixels[i]];

    this->screen_width = screen_width;
    this->screen_height = screen_height;
}

cc_bool EmulatorWidget::InputRequested(const cc_u8f player_id, const ClownMDEmu_Button button_id)
{
    return cc_false;
}

void EmulatorWidget::FMAudioToBeGenerated(const std::size_t total_frames, void (* const generate_fm_audio)(const ClownMDEmu *clownmdemu, cc_s16l *sample_buffer, std::size_t total_frames))
{

}

void EmulatorWidget::PSGAudioToBeGenerated(const std::size_t total_frames, void (* const generate_psg_audio)(const ClownMDEmu *clownmdemu, cc_s16l *sample_buffer, std::size_t total_frames))
{

}

void EmulatorWidget::PCMAudioToBeGenerated(const std::size_t total_frames, void (* const generate_pcm_audio)(const ClownMDEmu *clownmdemu, cc_s16l *sample_buffer, std::size_t total_frames))
{

}

void EmulatorWidget::CDDAAudioToBeGenerated(const std::size_t total_frames, void (* const generate_cdda_audio)(const ClownMDEmu *clownmdemu, cc_s16l *sample_buffer, std::size_t total_frames))
{

}

void EmulatorWidget::CDSeeked(const cc_u32f sector_index)
{

}

void EmulatorWidget::CDSectorRead(cc_u16l* const buffer)
{

}

cc_bool EmulatorWidget::CDTrackSeeked(const cc_u16f track_index, const ClownMDEmu_CDDAMode mode)
{
    return cc_false;
}

std::size_t EmulatorWidget::CDAudioRead(cc_s16l* const sample_buffer, const std::size_t total_frames)
{
    return 0;
}

cc_bool EmulatorWidget::SaveFileOpenedForReading(const char* const filename)
{
    return cc_false;
}

cc_s16f EmulatorWidget::SaveFileRead()
{
    return 0;
}

cc_bool EmulatorWidget::SaveFileOpenedForWriting(const char* const filename)
{
    return cc_false;
}

void EmulatorWidget::SaveFileWritten(const cc_u8f byte)
{

}

void EmulatorWidget::SaveFileClosed()
{

}

cc_bool EmulatorWidget::SaveFileRemoved(const char* const filename)
{
    return cc_false;
}

cc_bool EmulatorWidget::SaveFileSizeObtained(const char* const filename, std::size_t* const size)
{
    return cc_false;
}

void EmulatorWidget::timerEvent(QTimerEvent* const e)
{
    Iterate();

    makeCurrent();
    texture.bind();
    texture.setData(QOpenGLTexture::RGB, QOpenGLTexture::UInt16_R5G6B5, std::data(texture_buffer));
    doneCurrent();

    update();
}

void EmulatorWidget::LoadCartridgeSoftware(const QByteArray &cartridge_rom_buffer)
{
    timer.stop();

    this->cartridge_rom_buffer = cartridge_rom_buffer;
    state = {};
    Reset(cc_false, cartridge_rom_buffer.size());

    timer.start(std::chrono::nanoseconds(CLOWNMDEMU_DIVIDE_BY_NTSC_FRAMERATE(std::chrono::nanoseconds(std::chrono::seconds(1)).count())), Qt::TimerType::PreciseTimer, this);
}
