#ifndef WIDGETS_EMULATOR_H
#define WIDGETS_EMULATOR_H

#include <array>
#include <filesystem>
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

#include "../../emulator-extended.h"
#include "../../sdl-wrapper-inner.h"
#include "../../state-ring-buffer.h"
#include "../options.h"

namespace Widgets
{
	class Emulator : public QOpenGLWidget, protected QOpenGLFunctions, public EmulatorExtended
	{
		Q_OBJECT

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
		StateRingBuffer<State> state_rewind_buffer;

		const QVector<cc_u16l> &cartridge_rom_buffer;
		std::array<Colour, VDP_TOTAL_COLOURS> palette = {};
		struct
		{
			cc_u16f width, height;
			bool is_widescreen;
		} screen_properties;
		std::array<bool, CLOWNMDEMU_BUTTON_MAX> buttons = {};
		bool rewinding = false, fastforwarding = false, paused = false;

		// Emulator stuff.
		void ColourUpdated(cc_u16f index, cc_u16f colour) override;
		void ScanlineRendered(cc_u16f scanline, const cc_u8l *pixels, cc_u16f left_boundary, cc_u16f right_boundary, cc_u16f screen_width, cc_u16f screen_height) override;
		cc_bool InputRequested(cc_u8f player_id, ClownMDEmu_Button button_id) override;

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
		[[nodiscard]] bool DoButton(QKeyEvent *event, bool pressed);
		void Advance();

	public:
		explicit Emulator(const Options &options, const QVector<cc_u16l> &cartridge_rom_buffer, SDL::IOStream &&cd_stream, const std::filesystem::path &cd_path, QWidget* parent = nullptr, Qt::WindowFlags f = Qt::WindowFlags());

		~Emulator()
		{
			makeCurrent();
		}

		void SetRewindEnabled(const bool enabled)
		{
			state_rewind_buffer = StateRingBuffer<State>(enabled);
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
			EmulatorExtended::SoftReset();
		}

		void HardReset()
		{
			EmulatorExtended::HardReset();
		}
	};
}

#endif // WIDGETS_EMULATOR_H
