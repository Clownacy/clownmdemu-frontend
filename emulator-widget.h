#ifndef EMULATOR_WIDGET_H
#define EMULATOR_WIDGET_H

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

#include "emulator.h"

class EmulatorWidget : public QOpenGLWidget, protected QOpenGLFunctions, public Emulator
{
	Q_OBJECT

protected:
	using Base = QOpenGLWidget;

	static constexpr auto texture_buffer_width = 320;
	static constexpr auto texture_buffer_height = 480;

	QBasicTimer timer;

	std::optional<QOpenGLShaderProgram> shader_program;
//	QOpenGLVertexArrayObject vertex_array_object;
	QOpenGLBuffer vertex_buffer_object = QOpenGLBuffer(QOpenGLBuffer::VertexBuffer);
	QOpenGLTexture texture = QOpenGLTexture(QOpenGLTexture::Target2D);
	std::array<std::array<GLushort, texture_buffer_width>, texture_buffer_height> texture_buffer;

	static Configuration configuration;
	static Constant constant;
	State state;

	QByteArray cartridge_rom_buffer;
	std::array<GLushort, VDP_TOTAL_COLOURS> palette;
	cc_u16f screen_width, screen_height;
	std::array<bool, CLOWNMDEMU_BUTTON_MAX> buttons = {};

	// Emulator stuff
	unsigned char& AccessCartridgeBuffer(const std::size_t index)
	{
		// 'QByteArray' is signed, so we have to do some magic to treat it as unsigned.
		return reinterpret_cast<unsigned char*>(cartridge_rom_buffer.data())[index];
	}

	cc_u8f CartridgeRead(cc_u32f address) override;
	void CartridgeWritten(cc_u32f address, cc_u8f value) override;
	void ColourUpdated(cc_u16f index, cc_u16f colour) override;
	void ScanlineRendered(cc_u16f scanline, const cc_u8l *pixels, cc_u16f left_boundary, cc_u16f right_boundary, cc_u16f screen_width, cc_u16f screen_height) override;
	cc_bool InputRequested(cc_u8f player_id, ClownMDEmu_Button button_id) override;

	void FMAudioToBeGenerated(std::size_t total_frames, void (*generate_fm_audio)(const ClownMDEmu *clownmdemu, cc_s16l *sample_buffer, std::size_t total_frames)) override;
	void PSGAudioToBeGenerated(std::size_t total_frames, void (*generate_psg_audio)(const ClownMDEmu *clownmdemu, cc_s16l *sample_buffer, std::size_t total_frames)) override;
	void PCMAudioToBeGenerated(std::size_t total_frames, void (*generate_pcm_audio)(const ClownMDEmu *clownmdemu, cc_s16l *sample_buffer, std::size_t total_frames)) override;
	void CDDAAudioToBeGenerated(std::size_t total_frames, void (*generate_cdda_audio)(const ClownMDEmu *clownmdemu, cc_s16l *sample_buffer, std::size_t total_frames)) override;

	void CDSeeked(cc_u32f sector_index) override;
	void CDSectorRead(cc_u16l *buffer) override;
	cc_bool CDTrackSeeked(cc_u16f track_index, ClownMDEmu_CDDAMode mode) override;
	std::size_t CDAudioRead(cc_s16l *sample_buffer, std::size_t total_frames) override;

	cc_bool SaveFileOpenedForReading(const char *filename) override;
	cc_s16f SaveFileRead() override;
	cc_bool SaveFileOpenedForWriting(const char *filename) override;
	void SaveFileWritten(cc_u8f byte) override;
	void SaveFileClosed() override;
	cc_bool SaveFileRemoved(const char *filename) override;
	cc_bool SaveFileSizeObtained(const char *filename, std::size_t *size) override;

	// Qt stuff.
	void initializeGL() override;
	void paintGL() override;
	void timerEvent(QTimerEvent *event) override;
	void keyPressEvent(QKeyEvent *event) override;
	void keyReleaseEvent(QKeyEvent *event) override;

	// Miscellaneous.
	bool DoButton(QKeyEvent *event, bool pressed);

public:
	EmulatorWidget(QWidget* parent = nullptr, Qt::WindowFlags f = Qt::WindowFlags());
	~EmulatorWidget()
	{
		makeCurrent();
	}

	void LoadCartridgeSoftware(const QByteArray &cartridge_rom_buffer);

signals:

};

#endif // EMULATOR_WIDGET_H
