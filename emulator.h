#ifndef EMULATOR_H
#define EMULATOR_H

#include <array>
#include <optional>

#include <QBasicTimer>
#include <QByteArray>
#include <QOpenGLBuffer>
#include <QOpenGLVertexArrayObject>
#include <QOpenGLFunctions>
#include <QOpenGLShaderProgram>
#include <QOpenGLTexture>
#include <QOpenGLWidget>

#include "common/core/clownmdemu.h"

class Emulator : public QOpenGLWidget, protected QOpenGLFunctions
{
    Q_OBJECT

private:
    QBasicTimer timer;

    std::optional<QOpenGLShaderProgram> shader_program;
//    QOpenGLVertexArrayObject vertex_array_object;
    QOpenGLBuffer vertex_buffer_object = QOpenGLBuffer(QOpenGLBuffer::VertexBuffer);
    std::array<GLushort, VDP_TOTAL_COLOURS> palette_texture_buffer;
    QOpenGLTexture screen_texture = QOpenGLTexture(QOpenGLTexture::Target2D);
    std::array<std::array<GLushort, 320>, 480> screen_texture_buffer;

    ClownMDEmu_Configuration clownmdemu_configuration = {};
    ClownMDEmu_Constant clownmdemu_constant;
    ClownMDEmu_State clownmdemu_state;
    const ClownMDEmu_Callbacks clownmdemu_callbacks = {
        this,
        Callback_CartridgeRead,
        Callback_CartridgeWritten,
        Callback_ColourUpdated,
        Callback_ScanlineRendered,
        Callback_InputRequested,
        Callback_FMAudioToBeGenerated,
        Callback_PSGAudioToBeGenerated,
        Callback_PCMAudioToBeGenerated,
        Callback_CDDAAudioToBeGenerated,
        Callback_CDSeeked,
        Callback_CDSectorRead,
        Callback_CDTrackSeeked,
        Callback_CDAudioRead,
        Callback_SaveFileOpenedForReading,
        Callback_SaveFileRead,
        Callback_SaveFileOpenedForWriting,
        Callback_SaveFileWritten,
        Callback_SaveFileClosed,
        Callback_SaveFileRemoved,
        Callback_SaveFileSizeObtained
    };
    const ClownMDEmu clownmdemu = CLOWNMDEMU_PARAMETERS_INITIALISE(&clownmdemu_configuration, &clownmdemu_constant, &clownmdemu_state, &clownmdemu_callbacks);
    QByteArray cartridge_rom_buffer;

    unsigned char& AccessCartridgeBuffer(const std::size_t index)
    {
        // 'QByteArray' is signed, so we have to do some magic to treat it as unsigned.
        return reinterpret_cast<unsigned char*>(cartridge_rom_buffer.data())[index];
    }

    static cc_u8f Callback_CartridgeRead(void *user_data, cc_u32f address);
    static void Callback_CartridgeWritten(void *user_data, cc_u32f address, cc_u8f value);
    static void Callback_ColourUpdated(void *user_data, cc_u16f index, cc_u16f colour);
    static void Callback_ScanlineRendered(void *user_data, cc_u16f scanline, const cc_u8l *pixels, cc_u16f left_boundary, cc_u16f right_boundary, cc_u16f screen_width, cc_u16f screen_height);
    static cc_bool Callback_InputRequested(void *user_data, cc_u8f player_id, ClownMDEmu_Button button_id);

    static void Callback_FMAudioToBeGenerated(void *user_data, const ClownMDEmu *clownmdemu, std::size_t total_frames, void (*generate_fm_audio)(const ClownMDEmu *clownmdemu, cc_s16l *sample_buffer, std::size_t total_frames));
    static void Callback_PSGAudioToBeGenerated(void *user_data, const ClownMDEmu *clownmdemu, std::size_t total_frames, void (*generate_psg_audio)(const ClownMDEmu *clownmdemu, cc_s16l *sample_buffer, std::size_t total_frames));
    static void Callback_PCMAudioToBeGenerated(void *user_data, const ClownMDEmu *clownmdemu, std::size_t total_frames, void (*generate_pcm_audio)(const ClownMDEmu *clownmdemu, cc_s16l *sample_buffer, std::size_t total_frames));
    static void Callback_CDDAAudioToBeGenerated(void *user_data, const ClownMDEmu *clownmdemu, std::size_t total_frames, void (*generate_cdda_audio)(const ClownMDEmu *clownmdemu, cc_s16l *sample_buffer, std::size_t total_frames));

    static void Callback_CDSeeked(void *user_data, cc_u32f sector_index);
    static void Callback_CDSectorRead(void *user_data, cc_u16l *buffer);
    static cc_bool Callback_CDTrackSeeked(void *user_data, cc_u16f track_index, ClownMDEmu_CDDAMode mode);
    static std::size_t Callback_CDAudioRead(void *user_data, cc_s16l *sample_buffer, std::size_t total_frames);

    static cc_bool Callback_SaveFileOpenedForReading(void *user_data, const char *filename);
    static cc_s16f Callback_SaveFileRead(void *user_data);
    static cc_bool Callback_SaveFileOpenedForWriting(void *user_data, const char *filename);
    static void Callback_SaveFileWritten(void *user_data, cc_u8f byte);
    static void Callback_SaveFileClosed(void *user_data);
    static cc_bool Callback_SaveFileRemoved(void *user_data, const char *filename);
    static cc_bool Callback_SaveFileSizeObtained(void *user_data, const char *filename, std::size_t *size);

protected:
    void timerEvent(QTimerEvent *e) override;

    void initializeGL() override;
    void paintGL() override;

public:
    Emulator(QWidget* parent = nullptr, Qt::WindowFlags f = Qt::WindowFlags());
    ~Emulator();

    void LoadCartridgeSoftware(const QByteArray &cartridge_rom_buffer);

signals:

};

#endif // EMULATOR_H
