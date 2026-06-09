#ifndef THEME_H
#define THEME_H

#include <QColor>
#include <QPalette>

class QApplication;

// Central theme object (Phase 4). Both the QSS/QPalette-styled widget layer
// and the QPainter-drawn sequence view read their colors from here, so the
// look is defined in one place.
//
// Two modes: Modern (Fusion dark) and Classic (the original native BMS-editor
// look). The mode is chosen once at startup from settings; the grid/note
// painting and the palette all branch on it, so a user who prefers the classic
// look gets exactly the old appearance.
namespace Theme {

	// Selected once at startup (App reads it from settings). Restart-applied.
	void SetModern(bool modern);
	bool IsModern();

	// --- Elevation tiers (dark neutral surfaces, not pure black) ---
	inline QColor Surface()       { return QColor(0x1e, 0x1f, 0x22); } // base window
	inline QColor SurfaceAlt()    { return QColor(0x2b, 0x2d, 0x30); } // panels / buttons
	inline QColor SurfaceBright() { return QColor(0x38, 0x3a, 0x3e); } // hover / raised
	inline QColor SurfaceSunken() { return QColor(0x1b, 0x1c, 0x1e); } // inputs / tables

	inline QColor TextColor()     { return QColor(0xe6, 0xe6, 0xe6); }
	inline QColor TextMuted()     { return QColor(0x9a, 0x9c, 0xa0); }

	// One bright accent, reserved for selection and the playhead.
	inline QColor Accent()        { return QColor(0x4c, 0xa3, 0xff); }

	// --- Sequence-view grid tiers (mode-aware) ---
	QColor GridSubdiv();   // subdivisions, faint
	QColor GridBeat();     // beats, medium
	QColor GridMeasure();  // measure boundary, strong

	// Subtle per-measure alternating shading overlay (transparent in classic).
	QColor MeasureShade();

	// Categorical channel palette: roughly equal-luminance, slightly
	// desaturated hues (no pure primaries). Cycles for index >= count.
	QColor ChannelColor(int index);

	// A Fusion-based dark palette built from the surfaces above.
	QPalette DarkPalette();

	// Apply the chosen style: Fusion + dark palette in Modern; no-op (native)
	// in Classic.
	void Apply(QApplication *app);

}

#endif // THEME_H
