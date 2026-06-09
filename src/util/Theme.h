#ifndef THEME_H
#define THEME_H

#include <QColor>
#include <QPalette>

class QApplication;

// Central theme object (Phase 4). Both the QSS/QPalette-styled widget layer
// and the QPainter-drawn sequence view read their colors from here, so the
// look is defined in one place (and a future light/dark toggle becomes a
// matter of swapping the values returned below).
namespace Theme {

	// --- Elevation tiers (dark neutral surfaces, not pure black) ---
	inline QColor Surface()       { return QColor(0x1e, 0x1f, 0x22); } // base window
	inline QColor SurfaceAlt()    { return QColor(0x2b, 0x2d, 0x30); } // panels / buttons
	inline QColor SurfaceBright() { return QColor(0x38, 0x3a, 0x3e); } // hover / raised
	inline QColor SurfaceSunken() { return QColor(0x1b, 0x1c, 0x1e); } // inputs / tables

	inline QColor TextColor()     { return QColor(0xe6, 0xe6, 0xe6); }
	inline QColor TextMuted()     { return QColor(0x9a, 0x9c, 0xa0); }

	// One bright accent, reserved for selection and the playhead.
	inline QColor Accent()        { return QColor(0x4c, 0xa3, 0xff); }

	// --- Sequence-view grid tiers (recede the grid: faint -> strong) ---
	inline QColor GridSubdiv()    { return QColor(0x48, 0x4a, 0x4e); } // subdivisions, faint
	inline QColor GridBeat()      { return QColor(0x68, 0x6b, 0x70); } // beats, medium
	inline QColor GridMeasure()   { return QColor(0x96, 0x9a, 0xa2); } // measure boundary, strong

	// Subtle per-measure alternating shading overlay (low-alpha lightening).
	inline QColor MeasureShade()  { return QColor(0xff, 0xff, 0xff, 10); }

	// A Fusion-based dark palette built from the surfaces above.
	QPalette DarkPalette();

	// Apply the Fusion style + dark palette to the application.
	void Apply(QApplication *app);

}

#endif // THEME_H
