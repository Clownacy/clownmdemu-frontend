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

    if (!palette_texture.create())
        DisplayError("create palette texture");

    palette_texture.setFormat(QOpenGLTexture::RGBAFormat);
    palette_texture.setMinificationFilter(QOpenGLTexture::Nearest);
    palette_texture.setMagnificationFilter(QOpenGLTexture::Nearest);
    palette_texture.setSize(VDP_TOTAL_COLOURS, 1);
    palette_texture.allocateStorage(QOpenGLTexture::RGBA, QOpenGLTexture::UInt8);
    palette_texture.setData(QOpenGLTexture::RGBA, QOpenGLTexture::UInt8, std::data(palette_texture_buffer));

#undef DisplayError
}

void Emulator::paintGL()
{
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);

    shader_program->bind();
    vertex_buffer_object.bind();
    palette_texture.bind();

    shader_program->enableAttributeArray(attribute_name_vertex_position);
    shader_program->enableAttributeArray(attribute_name_vertex_texture_coordinate);

    glClear(GL_COLOR_BUFFER_BIT);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, std::size(vertices));
}

Emulator::Emulator(QWidget* const parent, const Qt::WindowFlags f)
    : QOpenGLWidget(parent, f)
{
    ClownMDEmu_Constant_Initialise(&clownmdemu_constant);
}

Emulator::~Emulator()
{
    makeCurrent();

    palette_texture.destroy();
    vertex_buffer_object.destroy();
    shader_program.reset();

    doneCurrent();
}

cc_u8f Emulator::Callback_CartridgeRead(void* const user_data, const cc_u32f address)
{
    Emulator &emulator = *static_cast<Emulator*>(user_data);

    if (address >= emulator.cartridge_rom_buffer.size())
        return 0;

    return emulator.AccessCartridgeBuffer(address);
}

void Emulator::Callback_CartridgeWritten(void* const user_data, const cc_u32f address, const cc_u8f value)
{
    Emulator &emulator = *static_cast<Emulator*>(user_data);

    if (address >= emulator.cartridge_rom_buffer.size())
        return;

    emulator.AccessCartridgeBuffer(address) = value;
}

void Emulator::Callback_ColourUpdated(void* const user_data, const cc_u16f index, const cc_u16f colour)
{
    Emulator &emulator = *static_cast<Emulator*>(user_data);

    for (unsigned int channel_index = 0; channel_index < 3; ++channel_index)
    {
        const cc_u8f channel = (colour >> ((channel_index * 4) + 1)) & 7;
        emulator.palette_texture_buffer[index][channel_index] = channel << 5 | channel << 2 | channel >> 1;
    }

    emulator.palette_texture_buffer[index][3] = 0xFF;
}

void Emulator::Callback_ScanlineRendered(void* const user_data, const cc_u16f scanline, const cc_u8l* const pixels, const cc_u16f left_boundary, const cc_u16f right_boundary, const cc_u16f screen_width, const cc_u16f screen_height)
{

}

cc_bool Emulator::Callback_InputRequested(void* const user_data, const cc_u8f player_id, const ClownMDEmu_Button button_id)
{
    return cc_false;
}

void Emulator::Callback_FMAudioToBeGenerated(void* const user_data, const ClownMDEmu* const clownmdemu, const std::size_t total_frames, void (* const generate_fm_audio)(const ClownMDEmu *clownmdemu, cc_s16l *sample_buffer, std::size_t total_frames))
{

}

void Emulator::Callback_PSGAudioToBeGenerated(void* const user_data, const ClownMDEmu* const clownmdemu, const std::size_t total_frames, void (* const generate_psg_audio)(const ClownMDEmu *clownmdemu, cc_s16l *sample_buffer, std::size_t total_frames))
{

}

void Emulator::Callback_PCMAudioToBeGenerated(void* const user_data, const ClownMDEmu* const clownmdemu, const std::size_t total_frames, void (* const generate_pcm_audio)(const ClownMDEmu *clownmdemu, cc_s16l *sample_buffer, std::size_t total_frames))
{

}

void Emulator::Callback_CDDAAudioToBeGenerated(void* const user_data, const ClownMDEmu* const clownmdemu, const std::size_t total_frames, void (* const generate_cdda_audio)(const ClownMDEmu *clownmdemu, cc_s16l *sample_buffer, std::size_t total_frames))
{

}

void Emulator::Callback_CDSeeked(void* const user_data, const cc_u32f sector_index)
{

}

void Emulator::Callback_CDSectorRead(void* const user_data, cc_u16l* const buffer)
{

}

cc_bool Emulator::Callback_CDTrackSeeked(void* const user_data, const cc_u16f track_index, const ClownMDEmu_CDDAMode mode)
{
    return cc_false;
}

std::size_t Emulator::Callback_CDAudioRead(void* const user_data, cc_s16l* const sample_buffer, const std::size_t total_frames)
{
    return 0;
}

cc_bool Emulator::Callback_SaveFileOpenedForReading(void* const user_data, const char* const filename)
{
    return cc_false;
}

cc_s16f Emulator::Callback_SaveFileRead(void* const user_data)
{
    return 0;
}

cc_bool Emulator::Callback_SaveFileOpenedForWriting(void* const user_data, const char* const filename)
{
    return cc_false;
}

void Emulator::Callback_SaveFileWritten(void* const user_data, const cc_u8f byte)
{

}

void Emulator::Callback_SaveFileClosed(void* const user_data)
{

}

cc_bool Emulator::Callback_SaveFileRemoved(void* const user_data, const char* const filename)
{
    return cc_false;
}

cc_bool Emulator::Callback_SaveFileSizeObtained(void* const user_data, const char* const filename, std::size_t* const size)
{
    return cc_false;
}

void Emulator::timerEvent(QTimerEvent* const e)
{
    ClownMDEmu_Iterate(&clownmdemu);

    makeCurrent();
    palette_texture.bind();
    palette_texture.setData(QOpenGLTexture::RGBA, QOpenGLTexture::UInt8, std::data(palette_texture_buffer));
    doneCurrent();

    update();
}

void Emulator::LoadCartridgeSoftware(const QByteArray &cartridge_rom_buffer)
{
    timer.stop();

    this->cartridge_rom_buffer = cartridge_rom_buffer;
    ClownMDEmu_State_Initialise(&clownmdemu_state);
    ClownMDEmu_Reset(&clownmdemu, cc_false, cartridge_rom_buffer.size());

    timer.start(std::chrono::nanoseconds(CLOWNMDEMU_DIVIDE_BY_NTSC_FRAMERATE(std::chrono::nanoseconds(std::chrono::seconds(1)).count())), Qt::TimerType::PreciseTimer, this);
}
