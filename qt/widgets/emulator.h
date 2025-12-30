#ifndef WIDGETS_EMULATOR_H
#define WIDGETS_EMULATOR_H

#include <array>
#include <filesystem>
#include <optional>
#include <type_traits>

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
#include "../../sdl-wrapper.h"
#include "../colour.h"
#include "../options.h"

namespace Widgets
{
	class Emulator : public QOpenGLWidget, protected QOpenGLFunctions, public EmulatorExtended<Colour>
	{
		Q_OBJECT

	protected:
		using Base = QOpenGLWidget;

		static constexpr auto texture_buffer_width = VDP_MAX_SCANLINE_WIDTH;
		static constexpr auto texture_buffer_height = VDP_MAX_SCANLINES;

		QBasicTimer timer;

		std::optional<QOpenGLShaderProgram> shader_program;
		QOpenGLVertexArrayObject  vertex_array_object;
		QOpenGLBuffer vertex_buffer_object = QOpenGLBuffer(QOpenGLBuffer::VertexBuffer);
		QOpenGLTexture texture = QOpenGLTexture(QOpenGLTexture::Target2D);
		std::array<std::array<Colour, texture_buffer_width>, texture_buffer_height> texture_buffer;

		const Options &options;

		struct
		{
			cc_u16f width, height;
			cc_u8f widescreen_tile_pairs;
		} screen_properties;
		std::array<bool, CLOWNMDEMU_BUTTON_MAX> buttons = {};
		bool paused = false;

		// Emulator stuff.
		void ScanlineRendered(cc_u16f scanline, const cc_u8l *pixels, cc_u16f left_boundary, cc_u16f right_boundary, cc_u16f screen_width, cc_u16f screen_height) override;
		cc_bool InputRequested(cc_u8f player_id, ClownMDEmu_Button button_id) override;

		// Qt stuff.
		void initializeGL() override;
		void paintGL() override;
		void timerEvent(QTimerEvent *event) override;
		void keyPressEvent(QKeyEvent *event) override;
		void keyReleaseEvent(QKeyEvent *event) override;

		// Miscellaneous.
		[[nodiscard]] bool DoButton(QKeyEvent *event, bool pressed);
		void Advance();

	private:
		void SetTimer(const bool pal_mode);

	public:
		explicit Emulator(const Options &options, const QVector<cc_u16l> &cartridge_rom_buffer, const std::filesystem::path &cartridge_path, SDL::IOStream &cd_stream, const std::filesystem::path &cd_path, QWidget* parent = nullptr, Qt::WindowFlags f = Qt::WindowFlags());

		~Emulator()
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

		void Reset(){ EmulatorExtended::Reset(); }
		void SetRewindEnabled(const bool enabled){ EmulatorExtended::SetRewindEnabled(enabled); }

		void SetTVStandard(const ClownMDEmu_TVStandard tv_standard)
		{
				EmulatorExtended::SetTVStandard(tv_standard);
				SetTimer(tv_standard == CLOWNMDEMU_TV_STANDARD_PAL);
		}
		void SetRegion(const ClownMDEmu_Region region){ EmulatorExtended::SetRegion(region); }
		void SetCDAddOnEnabled(const cc_bool enabled){ EmulatorExtended::SetCDAddOnEnabled(enabled); }
		void SetWidescreenTilePairs(const unsigned int tile_pairs){ EmulatorExtended::SetWidescreenTilePairs(tile_pairs); }
		void SetLowPassFilterEnabled(const cc_bool enabled){ EmulatorExtended::SetLowPassFilterEnabled(enabled); }
		void SetLadderEffectEnabled(const cc_bool enabled){ EmulatorExtended::SetLadderEffectEnabled(enabled); }

		void SetSpritePlaneEnabled(const cc_bool enabled){ EmulatorExtended::SetSpritePlaneEnabled(enabled); }
		void SetWindowPlaneEnabled(const cc_bool enabled){ EmulatorExtended::SetWindowPlaneEnabled(enabled); }
		void SetScrollPlaneAEnabled(const cc_bool enabled){ EmulatorExtended::SetScrollPlaneAEnabled(enabled); }
		void SetScrollPlaneBEnabled(const cc_bool enabled){ EmulatorExtended::SetScrollPlaneBEnabled(enabled); }
		void SetFM1Enabled(const cc_bool enabled){ EmulatorExtended::SetFM1Enabled(enabled); }
		void SetFM2Enabled(const cc_bool enabled){ EmulatorExtended::SetFM2Enabled(enabled); }
		void SetFM3Enabled(const cc_bool enabled){ EmulatorExtended::SetFM3Enabled(enabled); }
		void SetFM4Enabled(const cc_bool enabled){ EmulatorExtended::SetFM4Enabled(enabled); }
		void SetFM5Enabled(const cc_bool enabled){ EmulatorExtended::SetFM5Enabled(enabled); }
		void SetFM6Enabled(const cc_bool enabled){ EmulatorExtended::SetFM6Enabled(enabled); }
		void SetDACEnabled(const cc_bool enabled){ EmulatorExtended::SetDACEnabled(enabled); }
		void SetPSG1Enabled(const cc_bool enabled){ EmulatorExtended::SetPSG1Enabled(enabled); }
		void SetPSG2Enabled(const cc_bool enabled){ EmulatorExtended::SetPSG2Enabled(enabled); }
		void SetPSG3Enabled(const cc_bool enabled){ EmulatorExtended::SetPSG3Enabled(enabled); }
		void SetPSGNoiseEnabled(const cc_bool enabled){ EmulatorExtended::SetPSGNoiseEnabled(enabled); }
		void SetPCM1Enabled(const cc_bool enabled){ EmulatorExtended::SetPCM1Enabled(enabled); }
		void SetPCM2Enabled(const cc_bool enabled){ EmulatorExtended::SetPCM2Enabled(enabled); }
		void SetPCM3Enabled(const cc_bool enabled){ EmulatorExtended::SetPCM3Enabled(enabled); }
		void SetPCM4Enabled(const cc_bool enabled){ EmulatorExtended::SetPCM4Enabled(enabled); }
		void SetPCM5Enabled(const cc_bool enabled){ EmulatorExtended::SetPCM5Enabled(enabled); }
		void SetPCM6Enabled(const cc_bool enabled){ EmulatorExtended::SetPCM6Enabled(enabled); }
		void SetPCM7Enabled(const cc_bool enabled){ EmulatorExtended::SetPCM7Enabled(enabled); }
		void SetPCM8Enabled(const cc_bool enabled){ EmulatorExtended::SetPCM8Enabled(enabled); }
	};
}

#endif // WIDGETS_EMULATOR_H
