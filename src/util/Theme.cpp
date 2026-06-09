#include "Theme.h"
#include <QApplication>
#include <QStyleFactory>
#include <QVector>

namespace Theme {

static bool g_modern = true;

void SetModern(bool modern) { g_modern = modern; }
bool IsModern() { return g_modern; }

QColor GridSubdiv()
{
	return g_modern ? QColor(0x48, 0x4a, 0x4e) : QColor(90, 90, 90);
}

QColor GridBeat()
{
	return g_modern ? QColor(0x68, 0x6b, 0x70) : QColor(135, 135, 135);
}

QColor GridMeasure()
{
	return g_modern ? QColor(0x96, 0x9a, 0xa2) : QColor(180, 180, 180);
}

QColor MeasureShade()
{
	// Classic mode draws no alternating shading (fully transparent overlay).
	return g_modern ? QColor(0xff, 0xff, 0xff, 10) : QColor(0, 0, 0, 0);
}

QColor ChannelColor(int index)
{
	// 12 roughly equal-luminance, slightly-desaturated hues. Picked to read
	// well on a dark surface and to avoid garish pure-RGB primaries.
	static const QVector<QColor> palette = {
		QColor(0x6f, 0xb1, 0xff), // blue
		QColor(0x6f, 0xd1, 0x9a), // green
		QColor(0xe8, 0x9a, 0x5b), // orange
		QColor(0xc6, 0x8a, 0xe6), // purple
		QColor(0xe0, 0x8a, 0xaa), // pink
		QColor(0x6f, 0xcf, 0xd1), // teal
		QColor(0xd1, 0xc4, 0x6f), // gold
		QColor(0x9a, 0xb4, 0x6f), // olive
		QColor(0xe0, 0x7f, 0x7f), // salmon
		QColor(0x8f, 0x9a, 0xe6), // indigo
		QColor(0x6f, 0xc9, 0xb0), // aqua
		QColor(0xc9, 0x9a, 0x7f), // tan
	};
	if (index < 0)
		index = 0;
	return palette[index % palette.size()];
}

QPalette DarkPalette()
{
	QPalette p;
	const QColor surface = Surface();
	const QColor surfaceAlt = SurfaceAlt();
	const QColor sunken = SurfaceSunken();
	const QColor text = TextColor();
	const QColor disabled = TextMuted();
	const QColor accent = Accent();

	p.setColor(QPalette::Window, surface);
	p.setColor(QPalette::WindowText, text);
	p.setColor(QPalette::Base, sunken);
	p.setColor(QPalette::AlternateBase, surfaceAlt);
	p.setColor(QPalette::ToolTipBase, surfaceAlt);
	p.setColor(QPalette::ToolTipText, text);
	p.setColor(QPalette::Text, text);
	p.setColor(QPalette::Button, surfaceAlt);
	p.setColor(QPalette::ButtonText, text);
	p.setColor(QPalette::BrightText, QColor(0xff, 0x6b, 0x6b));
	p.setColor(QPalette::Link, accent);
	p.setColor(QPalette::Highlight, accent);
	p.setColor(QPalette::HighlightedText, QColor(0x10, 0x12, 0x16));

	// Disabled state: mute text and dim selection so inactive controls recede.
	p.setColor(QPalette::Disabled, QPalette::WindowText, disabled);
	p.setColor(QPalette::Disabled, QPalette::Text, disabled);
	p.setColor(QPalette::Disabled, QPalette::ButtonText, disabled);
	p.setColor(QPalette::Disabled, QPalette::Highlight, SurfaceBright());
	p.setColor(QPalette::Disabled, QPalette::HighlightedText, disabled);

	return p;
}

void Apply(QApplication *app)
{
	if (!app || !g_modern)
		return; // Classic mode keeps the platform's native style and palette.
	app->setStyle(QStyleFactory::create("Fusion"));
	app->setPalette(DarkPalette());
}

}
