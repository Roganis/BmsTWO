# Appearance

## Modern dark theme vs. Classic

BmsTWO ships a **Modern dark theme** (on by default) and a **Classic** mode that
restores the original BmsONE look exactly.

**How to use:** **Preferences → General → Modern dark theme** (requires
restart). Turn it off for Classic.

**What Modern changes:**

* **Fusion style + a centralized dark palette** applied app-wide.
* **Tiered gridlines** — faint subdivisions building up to strong measure
  lines — and subtle **alternating per-measure shading**, so dense charts stay
  readable.
* **Rounded note objects**; long notes read as connected **capsules**.
* **Per-channel color-coding** with equal-luminance categorical colors; notes in
  non-current channels are dimmed and the bright accent is reserved for the
  **selection / playhead**.
* **Panel polish** — row hover, slim rounded scrollbars, tighter menu/header/dock
  spacing, subtle dividers.
* **Monospace** for numeric / tick fields (note length, BGA/Stop tables, the
  Statistics and Selection panels).
* **High-DPI crispness** via pass-through device-pixel rounding.

**How it works:** A central `Theme` object is read by both the widget layer
(palette/stylesheet) and the painter (gridlines, notes). Classic mode simply
leaves the native style/palette in place and uses the original drawing path.

---

## Per-channel custom colors

**What:** Override a channel's note color — handy for reading a MOD dump's many
channels (e.g. drums red, piano green).

**How to use:** **Right-click a channel header → Set Color…** (or **Clear
Color** to revert to the automatic categorical color).

**How it works:** The color is stored on the channel as a non-standard
`x_color` `#RRGGBB` field that round-trips through save/load. The note painter
prefers a channel's custom color when set, falling back to the default lane
color otherwise. Changes are undoable.
