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
#include <QVector>

#include "../audio-output.h"
#include "emulator.h"

class EmulatorWidget : public QOpenGLWidget, protected QOpenGLFunctions, public Emulator
{
	Q_OBJECT

protected:
	using Base = QOpenGLWidget;

	class StateRingBuffer
	{
	protected:
		QVector<State> state_buffer;
		qsizetype state_buffer_index;
		qsizetype state_buffer_remaining;

		qsizetype NextIndex(qsizetype index) const
		{
			++index;

			if (index == std::size(state_buffer))
				index = 0;

			return index;
		}

		qsizetype PreviousIndex(qsizetype index) const
		{
			if (index == 0)
				index = std::size(state_buffer);

			--index;

			return index;
		}

	public:
		StateRingBuffer(const qsizetype size)
			: state_buffer(size)
		{
			assert(size > 0);

			static_cast<void>(Clear());
		}

		[[nodiscard]] State& Clear()
		{
			state_buffer_index = 0;
			state_buffer_remaining = 0;
			return state_buffer[state_buffer_index] = {};
		}

		[[nodiscard]] bool Exhausted() const
		{
			// We need at least two frames, because rewinding pops one frame and then samples the frame below the head.
			return state_buffer_remaining < 2;
		}

		[[nodiscard]] State& GetForward()
		{
			state_buffer_remaining = qMin(state_buffer_remaining + 1, std::size(state_buffer) - 2);

			const auto old_index = state_buffer_index;
			state_buffer_index = NextIndex(state_buffer_index);
			return state_buffer[state_buffer_index] = state_buffer[old_index];
		}

		[[nodiscard]] State& GetBackward()
		{
			assert(!Exhausted());
			--state_buffer_remaining;

			state_buffer_index = PreviousIndex(state_buffer_index);
			const auto old_index = PreviousIndex(state_buffer_index);
			return state_buffer[state_buffer_index] = state_buffer[old_index];
		}
	};

	static constexpr auto texture_buffer_width = 320;
	static constexpr auto texture_buffer_height = 480;

	QBasicTimer timer;

	std::optional<QOpenGLShaderProgram> shader_program;
	QOpenGLBuffer vertex_buffer_object = QOpenGLBuffer(QOpenGLBuffer::VertexBuffer);
	QOpenGLTexture texture = QOpenGLTexture(QOpenGLTexture::Target2D);
	std::array<std::array<GLushort, texture_buffer_width>, texture_buffer_height> texture_buffer;

	static Configuration configuration;
	static Constant constant;
	StateRingBuffer state_buffer = StateRingBuffer(10 * 60); // 10 seconds

	QByteArray cartridge_rom_buffer;
	std::array<GLushort, VDP_TOTAL_COLOURS> palette = {};
	cc_u16f screen_width, screen_height;
	std::array<bool, CLOWNMDEMU_BUTTON_MAX> buttons = {};
	bool rewinding = false, fastforwarding = false, paused = false;
	AudioOutput audio_output;

	// Emulator stuff.
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

	void FMAudioToBeGenerated(const ClownMDEmu *clownmdemu, std::size_t total_frames, void (*generate_fm_audio)(const ClownMDEmu *clownmdemu, cc_s16l *sample_buffer, std::size_t total_frames)) override;
	void PSGAudioToBeGenerated(const ClownMDEmu *clownmdemu, std::size_t total_frames, void (*generate_psg_audio)(const ClownMDEmu *clownmdemu, cc_s16l *sample_buffer, std::size_t total_frames)) override;
	void PCMAudioToBeGenerated(const ClownMDEmu *clownmdemu, std::size_t total_frames, void (*generate_pcm_audio)(const ClownMDEmu *clownmdemu, cc_s16l *sample_buffer, std::size_t total_frames)) override;
	void CDDAAudioToBeGenerated(const ClownMDEmu *clownmdemu, std::size_t total_frames, void (*generate_cdda_audio)(const ClownMDEmu *clownmdemu, cc_s16l *sample_buffer, std::size_t total_frames)) override;

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
	void Advance();

public:
	explicit EmulatorWidget(const QByteArray &cartridge_rom_buffer, QWidget* parent = nullptr, Qt::WindowFlags f = Qt::WindowFlags());

	~EmulatorWidget()
	{
		makeCurrent();
	}

signals:
	void NewFrame();

public slots:
	void Pause(const bool paused)
	{
		this->paused = paused;
	}

	void Reset()
	{
		Emulator::Reset(cc_false, std::size(cartridge_rom_buffer));
	}
};

#endif // EMULATOR_WIDGET_H
