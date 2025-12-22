#ifndef EMULATOR_WIDGET_H
#define EMULATOR_WIDGET_H

#include <array>
#include <optional>
#include <utility>

#include <QBasicTimer>
#include <QByteArray>
#include <QOpenGLBuffer>
#include <QOpenGLVertexArrayObject>
#include <QOpenGLFunctions>
#include <QOpenGLShaderProgram>
#include <QOpenGLTexture>
#include <QOpenGLWidget>
#include <QVector>

#include "../audio-output.h"
#include "../emulator.h"
#include "../state-ring-buffer.h"
#include "options.h"

class EmulatorWidget : public QOpenGLWidget, protected QOpenGLFunctions, public Emulator<EmulatorWidget>
{
	Q_OBJECT

	friend Emulator<EmulatorWidget>::Base;

protected:
	using Base = QOpenGLWidget;

	static constexpr auto texture_buffer_width = VDP_MAX_SCANLINE_WIDTH;
	static constexpr auto texture_buffer_height = VDP_MAX_SCANLINES;

	using Colour = GLushort;

	QBasicTimer timer;

	std::optional<QOpenGLShaderProgram> shader_program;
	QOpenGLVertexArrayObject  vertex_array_object;
	QOpenGLBuffer vertex_buffer_object = QOpenGLBuffer(QOpenGLBuffer::VertexBuffer);
	QOpenGLTexture texture = QOpenGLTexture(QOpenGLTexture::Target2D);
	std::array<std::array<Colour, texture_buffer_width>, texture_buffer_height> texture_buffer;

	const Options &options;
	StateRingBuffer<::ClownMDEmuCPP::State> state_rewind_buffer;

	QVector<cc_u16l> cartridge_rom_buffer;
	std::array<Colour, VDP_TOTAL_COLOURS> palette = {};
	struct
	{
		cc_u16f width, height;
		bool is_widescreen;
	} screen_properties;
	std::array<bool, CLOWNMDEMU_BUTTON_MAX> buttons = {};
	bool rewinding = false, fastforwarding = false, paused = false;
	AudioOutput audio_output;

	// Emulator stuff.
	void ColourUpdated(cc_u16f index, cc_u16f colour);
	void ScanlineRendered(cc_u16f scanline, const cc_u8l *pixels, cc_u16f left_boundary, cc_u16f right_boundary, cc_u16f screen_width, cc_u16f screen_height);
	cc_bool InputRequested(cc_u8f player_id, ClownMDEmu_Button button_id);

	void FMAudioToBeGenerated(const ClownMDEmu *clownmdemu, std::size_t total_frames, void (*generate_fm_audio)(const ClownMDEmu *clownmdemu, cc_s16l *sample_buffer, std::size_t total_frames));
	void PSGAudioToBeGenerated(const ClownMDEmu *clownmdemu, std::size_t total_frames, void (*generate_psg_audio)(const ClownMDEmu *clownmdemu, cc_s16l *sample_buffer, std::size_t total_frames));
	void PCMAudioToBeGenerated(const ClownMDEmu *clownmdemu, std::size_t total_frames, void (*generate_pcm_audio)(const ClownMDEmu *clownmdemu, cc_s16l *sample_buffer, std::size_t total_frames));
	void CDDAAudioToBeGenerated(const ClownMDEmu *clownmdemu, std::size_t total_frames, void (*generate_cdda_audio)(const ClownMDEmu *clownmdemu, cc_s16l *sample_buffer, std::size_t total_frames));

	void CDSeeked(cc_u32f sector_index);
	void CDSectorRead(cc_u16l *buffer);
	cc_bool CDTrackSeeked(cc_u16f track_index, ClownMDEmu_CDDAMode mode);
	std::size_t CDAudioRead(cc_s16l *sample_buffer, std::size_t total_frames);

	cc_bool SaveFileOpenedForReading(const char *filename);
	cc_s16f SaveFileRead();
	cc_bool SaveFileOpenedForWriting(const char *filename);
	void SaveFileWritten(cc_u8f byte);
	void SaveFileClosed();
	cc_bool SaveFileRemoved(const char *filename);
	cc_bool SaveFileSizeObtained(const char *filename, std::size_t *size);

	// Qt stuff.
	void initializeGL() override;
	void paintGL() override;
	void timerEvent(QTimerEvent *event) override;
	void keyPressEvent(QKeyEvent *event) override;
	void keyReleaseEvent(QKeyEvent *event) override;

	// Miscellaneous.
	[[nodiscard]] bool DoButton(QKeyEvent *event, bool pressed);
	void Advance();
	[[nodiscard]] std::pair<cc_u16f, cc_u16f> GetAspectRatio() const;

public:
	explicit EmulatorWidget(const Options &options, const QByteArray &cartridge_rom_buffer_bytes, QWidget* parent = nullptr, Qt::WindowFlags f = Qt::WindowFlags());

	~EmulatorWidget()
	{
		makeCurrent();
	}

	[[nodiscard]] bool IsCartridgeInserted() const
	{
		return !cartridge_rom_buffer.isEmpty();
	}

	void SetRewindEnabled(const bool enabled)
	{
		state_rewind_buffer = StateRingBuffer<::ClownMDEmuCPP::State>(enabled);
	}

signals:
	void NewFrame();

public slots:
	void Pause(const bool paused)
	{
		this->paused = paused;
	}

	void SoftReset()
	{
		Emulator::SoftReset(IsCartridgeInserted(), cc_false);
	}

	void HardReset()
	{
		Emulator::HardReset(IsCartridgeInserted(), cc_false);
	}
};

#endif // EMULATOR_WIDGET_H
