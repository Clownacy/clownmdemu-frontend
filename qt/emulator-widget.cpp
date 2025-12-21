#include "emulator-widget.h"

#include <array>

#include <QKeyEvent>
#include <QMessageBox>

struct Vertex
{
	std::array<GLbyte, 2> position;
};

static constexpr auto vertices = std::to_array<Vertex>({
	{{-0x80, -0x80}},
	{{ 0x7F, -0x80}},
	{{-0x80,  0x7F}},
	{{ 0x7F,  0x7F}}
});

static constexpr const char *attribute_name_vertex_position = "vertex_position";
static constexpr const char *attribute_name_vertex_texture_coordinate = "vertex_texture_coordinate";

void EmulatorWidget::initializeGL()
{
	initializeOpenGLFunctions();

#define DisplayError2(MESSAGE, EXTRA) QMessageBox::critical(this, "Fatal Error", "Failed to " MESSAGE "; emulator will not be displayed!" EXTRA)
#define DisplayError(MESSAGE) DisplayError2(MESSAGE, )

	shader_program.emplace();

	if (!shader_program->addShaderFromSourceFile(QOpenGLShader::Vertex, ":/vertex.glsl"))
		DisplayError2("load vertex shader", "\n" + shader_program->log());

	if (!shader_program->addShaderFromSourceFile(QOpenGLShader::Fragment, ":/fragment.glsl"))
		DisplayError2("load fragment shader", "\n" + shader_program->log());

	if (!shader_program->link())
		DisplayError("link shader program");

	if (!shader_program->bind())
		DisplayError("bind shader program");

	if (QOpenGLContext::openGLModuleType() == QOpenGLContext::LibGL)
	{
		if (!vertex_array_object.create())
			DisplayError("create vertex array object");

		vertex_array_object.bind();
	}

	if (!vertex_buffer_object.create())
		DisplayError("create vertex buffer object");

	vertex_buffer_object.bind();
	vertex_buffer_object.setUsagePattern(QOpenGLBuffer::StaticDraw);
	vertex_buffer_object.allocate(std::data(vertices), sizeof(vertices));

#define SetAttributeBuffer(PROGRAM, LOCATION, TYPE, STRUCT, MEMBER) (PROGRAM)->setAttributeBuffer(LOCATION, TYPE, offsetof(STRUCT, MEMBER), sizeof(STRUCT::MEMBER) / sizeof(STRUCT::MEMBER[0]), sizeof(STRUCT))

	SetAttributeBuffer(shader_program, attribute_name_vertex_position, GL_BYTE, Vertex, position);

#undef SetAttributeBuffer

	if (!texture.create())
		DisplayError("create screen texture");

	texture.setAutoMipMapGenerationEnabled(false);
	texture.setFormat(QOpenGLTexture::RGBFormat);
	texture.setMinificationFilter(QOpenGLTexture::Linear);
	texture.setMagnificationFilter(QOpenGLTexture::Linear);
	texture.setSize(texture_buffer_width, texture_buffer_height);
	texture.allocateStorage(QOpenGLTexture::RGB, QOpenGLTexture::UInt16_R5G6B5);

#undef DisplayError

	timer.start(std::chrono::nanoseconds(CLOWNMDEMU_DIVIDE_BY_NTSC_FRAMERATE(std::chrono::nanoseconds(std::chrono::seconds(1)).count())), Qt::TimerType::PreciseTimer, this);
}

std::pair<cc_u16f, cc_u16f> EmulatorWidget::GetAspectRatio() const
{
	cc_u16f x, y;

	if (screen_properties.is_widescreen)
		x = VDP_PAD_TILE_PAIRS_TO_WIDESCREEN(VDP_H40_SCREEN_WIDTH_IN_TILE_PAIRS) * VDP_TILE_PAIR_WIDTH;
	else
		x = VDP_H40_SCREEN_WIDTH_IN_TILE_PAIRS * VDP_TILE_PAIR_WIDTH;

	if (InterlaceMode2Enabled() && !options.TallInterlaceMode2Enabled())
		y = screen_properties.height / 2;
	else
		y = screen_properties.height;

	return {x, y};
}

void EmulatorWidget::paintGL()
{
	glClearColor(0.0f, 0.0f, 0.0f, 1.0f);

	shader_program->bind();
	vertex_buffer_object.bind();
	texture.bind();

	shader_program->enableAttributeArray(attribute_name_vertex_position);
	shader_program->enableAttributeArray(attribute_name_vertex_texture_coordinate);

	const cc_u16f output_width = width() * devicePixelRatio();
	const cc_u16f output_height = height() * devicePixelRatio();

	cc_u16f aspect_correct_width = 0, aspect_correct_height = 0;

	if (options.IntegerScreenScalingEnabled())
	{
		cc_u16f scale_factor_x, scale_factor_y;

		if (InterlaceMode2Enabled() && !options.TallInterlaceMode2Enabled())
		{
			const cc_u16f scale_factor = std::min(output_width / 2 / screen_properties.width, output_height / screen_properties.height);
			scale_factor_x = scale_factor * 2;
			scale_factor_y = scale_factor;
		}
		else
		{
			scale_factor_x = scale_factor_y = std::min(output_width / screen_properties.width, output_height / screen_properties.height);
		}

		aspect_correct_width = screen_properties.width * scale_factor_x;
		aspect_correct_height = screen_properties.height * scale_factor_y;
	}

	if (aspect_correct_width == 0 || aspect_correct_height == 0)
	{
		const auto &aspect_ratio = GetAspectRatio();

		if (static_cast<float>(output_width) / output_height > static_cast<float>(aspect_ratio.first) / aspect_ratio.second)
		{
			aspect_correct_height = output_height;
			aspect_correct_width = output_height * aspect_ratio.first / aspect_ratio.second;
		}
		else
		{
			aspect_correct_width = output_width;
			aspect_correct_height = output_width * aspect_ratio.second / aspect_ratio.first;
		}
	}

	shader_program->setUniformValue("OutputSize", QVector2D(output_width, output_height));
	shader_program->setUniformValue("OutputSizeAspectCorrected", QVector2D(aspect_correct_width, aspect_correct_height));
	shader_program->setUniformValue("TextureSize", QVector2D(texture_buffer_width, texture_buffer_height));
	shader_program->setUniformValue("InputSize", QVector2D(screen_properties.width, screen_properties.height));

	glClear(GL_COLOR_BUFFER_BIT);
	glDrawArrays(GL_TRIANGLE_STRIP, 0, std::size(vertices));
}

void EmulatorWidget::timerEvent([[maybe_unused]] QTimerEvent* const event)
{
	if (paused)
		return;

	Advance();
}

void EmulatorWidget::keyPressEvent(QKeyEvent* const event)
{
	if (!DoButton(event, true))
		Base::keyPressEvent(event);
}

void EmulatorWidget::keyReleaseEvent(QKeyEvent* const event)
{
	if (!DoButton(event, false))
		Base::keyReleaseEvent(event);
}

EmulatorWidget::EmulatorWidget(const Options &options, const QByteArray &cartridge_rom_buffer_bytes, QWidget* const parent, const Qt::WindowFlags f)
	: Base(parent, f)
	, options(options)
	, cartridge_rom_buffer(cartridge_rom_buffer_bytes.size() / sizeof(cc_u16l))
{
	// Enable keyboard input.
	setFocusPolicy(Qt::StrongFocus);

	// Convert the ROM buffer to 16-bit.
	const auto &GetByte = [&](const auto index) { return static_cast<cc_u16f>(cartridge_rom_buffer_bytes[index]) & 0xFF; };

	for (qsizetype i = 0; i < std::size(cartridge_rom_buffer); ++i)
	{
		cartridge_rom_buffer[i]
			= (GetByte(i * 2 + 0) << 8)
			| (GetByte(i * 2 + 1) << 0);
	}

	// Initialise the emulator.
	SetParameters(options.GetEmulatorConfiguration(), state);
	// TODO: Merge this with 'SetParameters'.
	SetCartridge(std::data(cartridge_rom_buffer), std::size(cartridge_rom_buffer));
	Reset();
}

void EmulatorWidget::ColourUpdated(const cc_u16f index, const cc_u16f colour)
{
	const auto &Extract4BitChannelTo6Bit = [&](const unsigned int channel_index)
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

	screen_properties.width = screen_width;
	screen_properties.height = screen_height;
	screen_properties.is_widescreen = options.WidescreenEnabled();
}

cc_bool EmulatorWidget::InputRequested(const cc_u8f player_id, const ClownMDEmu_Button button_id)
{
	// TODO: Player 2.
	if (player_id != options.KeyboardControlPad())
		return cc_false;

	return buttons[button_id];
}

void EmulatorWidget::FMAudioToBeGenerated(const ClownMDEmu* const clownmdemu, const std::size_t total_frames, void (* const generate_fm_audio)(const ClownMDEmu *clownmdemu, cc_s16l *sample_buffer, std::size_t total_frames))
{
	generate_fm_audio(clownmdemu, audio_output.MixerAllocateFMSamples(total_frames), total_frames);
}

void EmulatorWidget::PSGAudioToBeGenerated(const ClownMDEmu* const clownmdemu, const std::size_t total_frames, void (* const generate_psg_audio)(const ClownMDEmu *clownmdemu, cc_s16l *sample_buffer, std::size_t total_frames))
{
	generate_psg_audio(clownmdemu, audio_output.MixerAllocatePSGSamples(total_frames), total_frames);
}

void EmulatorWidget::PCMAudioToBeGenerated(const ClownMDEmu* const clownmdemu, const std::size_t total_frames, void (* const generate_pcm_audio)(const ClownMDEmu *clownmdemu, cc_s16l *sample_buffer, std::size_t total_frames))
{
	generate_pcm_audio(clownmdemu, audio_output.MixerAllocatePCMSamples(total_frames), total_frames);
}

void EmulatorWidget::CDDAAudioToBeGenerated(const ClownMDEmu* const clownmdemu, const std::size_t total_frames, void (* const generate_cdda_audio)(const ClownMDEmu *clownmdemu, cc_s16l *sample_buffer, std::size_t total_frames))
{
	generate_cdda_audio(clownmdemu, audio_output.MixerAllocateCDDASamples(total_frames), total_frames);
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

bool EmulatorWidget::DoButton(QKeyEvent* const event, const bool pressed)
{
	if (event->isAutoRepeat())
		return false;

	switch (event->key())
	{
#define DO_KEY(KEY, VARIABLE) \
		case KEY: \
			VARIABLE = pressed; \
			break

#define DO_KEY_BUTTON(KEY, BUTTON) DO_KEY(KEY, buttons[BUTTON])

		DO_KEY_BUTTON(Qt::Key_W, CLOWNMDEMU_BUTTON_UP);
		DO_KEY_BUTTON(Qt::Key_A, CLOWNMDEMU_BUTTON_LEFT);
		DO_KEY_BUTTON(Qt::Key_S, CLOWNMDEMU_BUTTON_DOWN);
		DO_KEY_BUTTON(Qt::Key_D, CLOWNMDEMU_BUTTON_RIGHT);
		DO_KEY_BUTTON(Qt::Key_O, CLOWNMDEMU_BUTTON_A);
		DO_KEY_BUTTON(Qt::Key_P, CLOWNMDEMU_BUTTON_B);
		DO_KEY_BUTTON(Qt::Key_BracketLeft, CLOWNMDEMU_BUTTON_C);
		DO_KEY_BUTTON(Qt::Key_9, CLOWNMDEMU_BUTTON_X);
		DO_KEY_BUTTON(Qt::Key_0, CLOWNMDEMU_BUTTON_Y);
		DO_KEY_BUTTON(Qt::Key_Minus, CLOWNMDEMU_BUTTON_Z);
		DO_KEY_BUTTON(Qt::Key_Return, CLOWNMDEMU_BUTTON_START);
		DO_KEY_BUTTON(Qt::Key_Backspace, CLOWNMDEMU_BUTTON_MODE);

#undef DO_KEY_BUTTON

		DO_KEY(Qt::Key_R, rewinding);

#undef DO_KEY

		case Qt::Key_Space:
			fastforwarding = pressed;

			if (pressed && paused)
				Advance();

			break;

		default:
			return false;
	}

	return true;
}

void EmulatorWidget::Advance()
{
	for (unsigned int i = 0; i < (fastforwarding && !paused ? 3 : 1); ++i)
	{
		if (rewinding)
		{
			if (state_rewind_buffer.Exhausted())
				return;

			state = state_rewind_buffer.GetBackward();
		}
		else
		{
			state_rewind_buffer.GetForward() = state;
		}

		audio_output.MixerBegin();
		Iterate();
		audio_output.MixerEnd();
	}

	makeCurrent();
	texture.bind();
	texture.setData(QOpenGLTexture::RGB, QOpenGLTexture::UInt16_R5G6B5, std::data(texture_buffer));
	doneCurrent();

	update();

	emit NewFrame();
}
