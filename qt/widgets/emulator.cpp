#include "emulator.h"

#include <algorithm>
#include <array>

#include <QKeyEvent>
#include <QMessageBox>
#include <QStandardPaths>

#include "../qt-extensions.h"

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

void Widgets::Emulator::initializeGL()
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

	SetTimer(GetTVStandard() == CLOWNMDEMU_TV_STANDARD_PAL);
}

void Widgets::Emulator::paintGL()
{
	glClearColor(0.0f, 0.0f, 0.0f, 1.0f);

	shader_program->bind();
	vertex_buffer_object.bind();
	texture.bind();

	shader_program->enableAttributeArray(attribute_name_vertex_position);
	shader_program->enableAttributeArray(attribute_name_vertex_texture_coordinate);

	const bool squashed = GetState().vdp.double_resolution_enabled && !options.GetTallInterlaceMode2Enabled();

	const cc_u16f output_width = width() * devicePixelRatio();
	const cc_u16f output_height = height() * devicePixelRatio();

	cc_u16f aspect_correct_width = 0, aspect_correct_height = 0;

	if (options.GetIntegerScreenScalingEnabled())
	{
		cc_u16f scale_factor_x, scale_factor_y;

		if (squashed)
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
		const auto &GetAspectRatio = [&]()
		{
			const cc_u16f x = (VDP_H40_SCREEN_WIDTH_IN_TILE_PAIRS + screen_properties.widescreen_tile_pairs * 2) * VDP_TILE_PAIR_WIDTH;
			const cc_u16f y = squashed ? screen_properties.height / 2 : screen_properties.height;

			return std::make_pair(x, y);
		};

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

void Widgets::Emulator::timerEvent([[maybe_unused]] QTimerEvent* const event)
{
	if (paused)
		return;

	Advance();
}

void Widgets::Emulator::keyPressEvent(QKeyEvent* const event)
{
	if (!DoButton(event, true))
		Base::keyPressEvent(event);
}

void Widgets::Emulator::keyReleaseEvent(QKeyEvent* const event)
{
	if (!DoButton(event, false))
		Base::keyReleaseEvent(event);
}

static std::filesystem::path GetSaveDataDirectoryPath()
{
	return QtExtensions::toStdPath(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)) / "Save Data";
}

Widgets::Emulator::Emulator(const Options &options, const QVector<cc_u16l> &cartridge_rom_buffer, const std::filesystem::path &cartridge_path, SDL::IOStream &cd_stream, const std::filesystem::path &cd_path, QWidget* const parent, const Qt::WindowFlags f)
	: Base(parent, f)
	, EmulatorExtended(options.GetEmulatorConfiguration(), options.GetRewindingEnabled(), GetSaveDataDirectoryPath())
	, options(options)
{
	// Enable keyboard input.
	setFocusPolicy(Qt::StrongFocus);

	// TODO: Merge this with 'SetParameters'?
	if (!cartridge_rom_buffer.isEmpty())
		InsertCartridge(cartridge_path, std::data(cartridge_rom_buffer), std::size(cartridge_rom_buffer));

	if (cd_stream != nullptr)
		InsertCD(cd_stream, cd_path);

	Reset();
}

void Widgets::Emulator::ScanlineRendered(const cc_u16f scanline, const cc_u8l* const pixels, const cc_u16f left_boundary, const cc_u16f right_boundary, const cc_u16f screen_width, const cc_u16f screen_height)
{
	for (cc_u16f i = 0; i < right_boundary - left_boundary; ++i)
		texture_buffer[scanline][left_boundary + i] = GetColour(pixels[i]);

	screen_properties.width = screen_width;
	screen_properties.height = screen_height;
	screen_properties.widescreen_tile_pairs = GetWidescreenTilePairs();
}

cc_bool Widgets::Emulator::InputRequested(const cc_u8f player_id, const ClownMDEmu_Button button_id)
{
	// TODO: Player 2.
	if (player_id != options.GetKeyboardControlPad())
		return cc_false;

	return buttons[button_id];
}

bool Widgets::Emulator::DoButton(QKeyEvent* const event, const bool pressed)
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

void Widgets::Emulator::Advance()
{
	// If the emulator has not updated at all, then don't bother redrawing the screen.
	if (!Iterate())
		return;

	makeCurrent();
	texture.bind();
	texture.setData(QOpenGLTexture::RGB, QOpenGLTexture::UInt16_R5G6B5, std::data(texture_buffer));
	doneCurrent();

	update();

	emit NewFrame();
}

void Widgets::Emulator::SetTimer(const bool pal_mode)
{
	const auto time_ntsc = CLOWNMDEMU_DIVIDE_BY_NTSC_FRAMERATE(std::chrono::nanoseconds(std::chrono::seconds(1)).count());
	const auto time_pal = CLOWNMDEMU_DIVIDE_BY_PAL_FRAMERATE(std::chrono::nanoseconds(std::chrono::seconds(1)).count());
	timer.start(std::chrono::nanoseconds(pal_mode ? time_pal : time_ntsc), Qt::TimerType::PreciseTimer, this);
}
