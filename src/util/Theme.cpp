#include "Theme.h"
#include <QApplication>
#include <QStyleFactory>

namespace Theme {

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
	if (!app)
		return;
	app->setStyle(QStyleFactory::create("Fusion"));
	app->setPalette(DarkPalette());
}

}
